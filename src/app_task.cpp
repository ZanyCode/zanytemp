/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "app_task.h"

#include "app/matter_init.h"
#include "app/task_executor.h"
#include "board/board.h"
#include "clusters/identify.h"
#include "lib/core/CHIPError.h"

#include <app-common/zap-generated/attributes/Accessors.h>

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app, CONFIG_CHIP_APP_LOG_LEVEL);

using namespace ::chip;
using namespace ::chip::app;
using namespace ::chip::DeviceLayer;

namespace
{
constexpr chip::EndpointId kTemperatureSensorEndpointId = 1;

Nrf::Matter::IdentifyCluster sIdentifyCluster(kTemperatureSensorEndpointId);

constexpr uint32_t kFactoryResetTriggerMs = 3000;
constexpr Nrf::ButtonMask kFactoryResetButtonMask = DK_BTN1_MSK;

#ifdef CONFIG_CHIP_ICD_UAT_SUPPORT
#define UAT_BUTTON_MASK 1
#endif
} /* namespace */

void AppTask::RunHeaterCycle()
{
	/* SHT41 heater command bytes: low/short, med/short, med/long */
	static const uint8_t heater_cmds[] = { 0x15, 0x24, 0x2F };
	static const struct i2c_dt_spec sht41_i2c = I2C_DT_SPEC_GET(DT_NODELABEL(sht41));

	uint8_t cmd = heater_cmds[mHeaterEscalationLevel];
	int rc = i2c_write_dt(&sht41_i2c, &cmd, 1);
	if (rc) {
		LOG_ERR("Heater command failed: %d", rc);
	} else {
		LOG_INF("Heater fired (level %d, cmd 0x%02x)", mHeaterEscalationLevel, cmd);
	}

	if (mHeaterEscalationLevel < 2) {
		mHeaterEscalationLevel++;
	}
}

void AppTask::FactoryResetTimerCallback(k_timer *)
{
	DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t) {
		LOG_INF("Factory reset triggered.");
		chip::Server::GetInstance().ScheduleFactoryReset();
	}, 0);
}

void AppTask::ButtonEventHandler(Nrf::ButtonState state, Nrf::ButtonMask hasChanged)
{
	if (kFactoryResetButtonMask & hasChanged) {
		if (kFactoryResetButtonMask & state) {
			LOG_INF("Factory reset button pressed, hold 3s to reset.");
			k_timer_start(&Instance().mFactoryResetTimer, K_MSEC(kFactoryResetTriggerMs), K_NO_WAIT);
		} else {
			LOG_INF("Factory reset cancelled.");
			k_timer_stop(&Instance().mFactoryResetTimer);
		}
	}
}

void AppTask::UpdateSensorTimeoutCallback(k_timer *timer)
{
	if (!timer || !timer->user_data) {
		return;
	}

	DeviceLayer::PlatformMgr().ScheduleWork(
		[](intptr_t p) {
			AppTask &app = AppTask::Instance();
			const struct device *dev = app.mSht4xDev;

			int err = sensor_sample_fetch(dev);
			if (err) {
				LOG_ERR("Failed to fetch SHT41 sample: %d", err);
				return;
			}

			struct sensor_value temp_val, hum_val;
			sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_val);
			sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &hum_val);

			/* Matter temperature cluster: int16_t = 100 × °C (0.01 °C resolution) */
			int16_t temperature =
				(int16_t)(temp_val.val1 * 100 + temp_val.val2 / 10000);

			/* Matter humidity cluster: uint16_t = 100 × %RH (0.01 %RH resolution) */
			uint16_t humidity =
				(uint16_t)(hum_val.val1 * 100 + hum_val.val2 / 10000);

			/* Anti-condensation heater state machine */
			if (app.mHeaterState == HeaterState::Idle) {
				/* 6500 = 65 °C in Matter units; SHT41 heater must not run above that */
				if (temperature < 6500 &&
				    humidity >= kHighHumidityThreshold) {
					app.mHighHumidityCount++;
					if (app.mHighHumidityCount >= kCondensationTriggerCount) {
						app.RunHeaterCycle();
						app.mHeaterState        = HeaterState::Cooldown;
						app.mCooldownCyclesLeft = kHeaterCooldownCycles;
						app.mHighHumidityCount  = 0;
					}
				} else {
					app.mHighHumidityCount     = 0;
					app.mHeaterEscalationLevel = 0;
				}
			} else { /* Cooldown */
				app.mCooldownCyclesLeft--;
				if (humidity < kHighHumidityThreshold) {
					app.mHeaterEscalationLevel = 0;
				}
				if (app.mCooldownCyclesLeft == 0) {
					app.mHeaterState = HeaterState::Idle;
				}
			}

			if (app.mHeaterState == HeaterState::Cooldown) {
				LOG_INF("Temperature: %d.%02d °C, Humidity: %d.%02d %% [heater cooldown %d/%d]",
					temperature / 100, abs(temperature % 100),
					humidity / 100, humidity % 100,
					kHeaterCooldownCycles - app.mCooldownCyclesLeft,
					kHeaterCooldownCycles);
			} else if (app.mHighHumidityCount > 0) {
				LOG_INF("Temperature: %d.%02d °C, Humidity: %d.%02d %% [high RH %d/%d]",
					temperature / 100, abs(temperature % 100),
					humidity / 100, humidity % 100,
					app.mHighHumidityCount, kCondensationTriggerCount);
			} else {
				LOG_INF("Temperature: %d.%02d °C, Humidity: %d.%02d %%",
					temperature / 100, abs(temperature % 100),
					humidity / 100, humidity % 100);
			}

			Protocols::InteractionModel::Status status =
				Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(
					kTemperatureSensorEndpointId, temperature);
			if (status != Protocols::InteractionModel::Status::Success) {
				LOG_ERR("Updating temperature measurement failed %x",
					to_underlying(status));
			}

			status = Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Set(
				kTemperatureSensorEndpointId, humidity);
			if (status != Protocols::InteractionModel::Status::Success) {
				LOG_ERR("Updating humidity measurement failed %x",
					to_underlying(status));
			}
		},
		reinterpret_cast<intptr_t>(timer->user_data));
}

CHIP_ERROR AppTask::Init()
{
	k_timer_init(&mFactoryResetTimer, FactoryResetTimerCallback, nullptr);

	mSht4xDev = DEVICE_DT_GET(DT_NODELABEL(sht41));
	if (!device_is_ready(mSht4xDev)) {
		LOG_ERR("SHT41 sensor not ready");
		return CHIP_ERROR_INCORRECT_STATE;
	}

	/* Initialize Matter stack */
	ReturnErrorOnFailure(Nrf::Matter::PrepareServer());

	if (!Nrf::GetBoard().Init(ButtonEventHandler)) {
		LOG_ERR("User interface initialization failed.");
		return CHIP_ERROR_INCORRECT_STATE;
	}

	/* Register Matter event handler that controls the connectivity status LED based on the captured Matter network
	 * state. */
	ReturnErrorOnFailure(Nrf::Matter::RegisterEventHandler(Nrf::Board::DefaultMatterEventHandler, 0));

	ReturnErrorOnFailure(sIdentifyCluster.Init());

	return Nrf::Matter::StartServer();
}

CHIP_ERROR AppTask::StartApp()
{
	ReturnErrorOnFailure(Init());

	k_timer_init(&mTimer, AppTask::UpdateSensorTimeoutCallback, nullptr);
	k_timer_user_data_set(&mTimer, this);
	k_timer_start(&mTimer, K_MSEC(kMeasurementIntervalMs), K_MSEC(kMeasurementIntervalMs));

	while (true) {
		Nrf::DispatchNextTask();
	}

	return CHIP_NO_ERROR;
}
