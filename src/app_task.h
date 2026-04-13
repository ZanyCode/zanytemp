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

	static constexpr uint16_t kMeasurementIntervalMs = 10000; /* 10 seconds */

	static void UpdateSensorTimeoutCallback(k_timer *timer);
	static void FactoryResetTimerCallback(k_timer *timer);

	static void ButtonEventHandler(Nrf::ButtonState state, Nrf::ButtonMask hasChanged);

	const struct device *mSht4xDev = nullptr;
};
