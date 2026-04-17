/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once

#include "board/board.h"

#include <platform/CHIPDeviceLayer.h>
#include <zephyr/device.h>

struct Identify;

class AppTask {
public:
	static AppTask &Instance()
	{
		static AppTask sAppTask;
		return sAppTask;
	};

	CHIP_ERROR StartApp();

private:
	CHIP_ERROR Init();
	k_timer mTimer;
	k_timer mFactoryResetTimer;

	static constexpr uint16_t kMeasurementIntervalMs = 30000; /* 10 seconds */

	static void UpdateSensorTimeoutCallback(k_timer *timer);
	static void FactoryResetTimerCallback(k_timer *timer);

	static void ButtonEventHandler(Nrf::ButtonState state, Nrf::ButtonMask hasChanged);

	void RunHeaterCycle();

	const struct device *mSht4xDev = nullptr;

	/* Anti-condensation heater state */
	static constexpr uint16_t kHighHumidityThreshold  = 8500; /* 85.00 %RH in Matter units */
	static constexpr uint8_t  kCondensationTriggerCount = 3;  /* consecutive readings before heating */
	static constexpr uint8_t  kHeaterCooldownCycles    = 6;   /* measurement cycles to wait after heating */

	enum class HeaterState : uint8_t { Idle, Cooldown };
	HeaterState mHeaterState         = HeaterState::Idle;
	uint8_t     mHighHumidityCount   = 0;
	uint8_t     mCooldownCyclesLeft  = 0;
	uint8_t     mHeaterEscalationLevel = 0;
};
