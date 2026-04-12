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

void AppTask::UpdateTemperatureTimeoutCallback(k_timer *timer)
{
	if (!timer || !timer->user_data) {
		return;
	}

	DeviceLayer::PlatformMgr().ScheduleWork(
		[](intptr_t p) {
			AppTask::Instance().UpdateTemperatureMeasurement();

			int16_t temperature = AppTask::Instance().GetCurrentTemperature();
			LOG_INF("Temperature: %d.%02d °C", temperature / 100, temperature % 100);

			Protocols::InteractionModel::Status status =
				Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(
					kTemperatureSensorEndpointId, temperature);

			if (status != Protocols::InteractionModel::Status::Success) {
				LOG_ERR("Updating temperature measurement failed %x", to_underlying(status));
			}
		},
		reinterpret_cast<intptr_t>(timer->user_data));
}

void AppTask::UpdateHumidityTimeoutCallback(k_timer *timer)
{
	if (!timer || !timer->user_data) {
		return;
	}

	DeviceLayer::PlatformMgr().ScheduleWork(
		[](intptr_t p) {
			AppTask::Instance().UpdateHumidityMeasurement();

			uint16_t humidity = AppTask::Instance().GetCurrentHumidity();
			LOG_INF("Humidity: %d.%02d %%", humidity / 100, humidity % 100);

			Protocols::InteractionModel::Status status =
				Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Set(
					kTemperatureSensorEndpointId, humidity);

			if (status != Protocols::InteractionModel::Status::Success) {
				LOG_ERR("Updating humidity measurement failed %x", to_underlying(status));
			}
		},
		reinterpret_cast<intptr_t>(timer->user_data));
}

CHIP_ERROR AppTask::Init()
{
	k_timer_init(&mFactoryResetTimer, FactoryResetTimerCallback, nullptr);

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

	DataModel::Nullable<int16_t> val;
	Protocols::InteractionModel::Status status =
		Clusters::TemperatureMeasurement::Attributes::MinMeasuredValue::Get(kTemperatureSensorEndpointId, val);

	if (status != Protocols::InteractionModel::Status::Success || val.IsNull()) {
		LOG_ERR("Failed to get temperature measurement min value %x", to_underlying(status));
		return CHIP_ERROR_INCORRECT_STATE;
	}

	mTemperatureSensorMinValue = val.Value();

	status = Clusters::TemperatureMeasurement::Attributes::MaxMeasuredValue::Get(kTemperatureSensorEndpointId, val);

	if (status != Protocols::InteractionModel::Status::Success || val.IsNull()) {
		LOG_ERR("Failed to get temperature measurement max value %x", to_underlying(status));
		return CHIP_ERROR_INCORRECT_STATE;
	}

	mTemperatureSensorMaxValue = val.Value();

	k_timer_init(&mTimer, AppTask::UpdateTemperatureTimeoutCallback, nullptr);
	k_timer_user_data_set(&mTimer, this);
	k_timer_start(&mTimer, K_MSEC(kTemperatureMeasurementIntervalMs), K_MSEC(kTemperatureMeasurementIntervalMs));

	DataModel::Nullable<uint16_t> humVal;
	status = Clusters::RelativeHumidityMeasurement::Attributes::MinMeasuredValue::Get(kTemperatureSensorEndpointId,
										       humVal);

	if (status != Protocols::InteractionModel::Status::Success || humVal.IsNull()) {
		LOG_ERR("Failed to get humidity measurement min value %x", to_underlying(status));
		return CHIP_ERROR_INCORRECT_STATE;
	}

	mHumiditySensorMinValue = humVal.Value();

	status = Clusters::RelativeHumidityMeasurement::Attributes::MaxMeasuredValue::Get(kTemperatureSensorEndpointId,
										       humVal);

	if (status != Protocols::InteractionModel::Status::Success || humVal.IsNull()) {
		LOG_ERR("Failed to get humidity measurement max value %x", to_underlying(status));
		return CHIP_ERROR_INCORRECT_STATE;
	}

	mHumiditySensorMaxValue = humVal.Value();

	k_timer_init(&mHumidityTimer, AppTask::UpdateHumidityTimeoutCallback, nullptr);
	k_timer_user_data_set(&mHumidityTimer, this);
	k_timer_start(&mHumidityTimer, K_MSEC(kTemperatureMeasurementIntervalMs),
		      K_MSEC(kTemperatureMeasurementIntervalMs));

	while (true) {
		Nrf::DispatchNextTask();
	}

	return CHIP_NO_ERROR;
}
