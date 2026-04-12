/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once

#include "board/board.h"

#include <platform/CHIPDeviceLayer.h>

struct Identify;

class AppTask {
public:
	static AppTask &Instance()
	{
		static AppTask sAppTask;
		return sAppTask;
	};

	CHIP_ERROR StartApp();

	/* Defined by cluster temperature measured value = 100 x temperature in degC with resolution of
	 * 0.01 degC. */
	void UpdateTemperatureMeasurement()
	{
		/* Linear temperature increase that is wrapped around to min value after reaching the max value. */
		if (mCurrentTemperature < mTemperatureSensorMaxValue) {
			mCurrentTemperature += kTemperatureMeasurementStep;
		} else {
			mCurrentTemperature = mTemperatureSensorMinValue;
		}
	}

	int16_t GetCurrentTemperature() const { return mCurrentTemperature; }

	/* Defined by cluster relative humidity measured value = 100 x humidity in % with resolution of
	 * 0.01 %. */
	void UpdateHumidityMeasurement()
	{
		/* Linear humidity increase that is wrapped around to min value after reaching the max value. */
		if (mCurrentHumidity < mHumiditySensorMaxValue) {
			mCurrentHumidity += kHumidityMeasurementStep;
		} else {
			mCurrentHumidity = mHumiditySensorMinValue;
		}
	}

	uint16_t GetCurrentHumidity() const { return mCurrentHumidity; }

private:
	CHIP_ERROR Init();
	k_timer mTimer;
	k_timer mFactoryResetTimer;

	static constexpr uint16_t kTemperatureMeasurementIntervalMs = 10000; /* 10 seconds */
	static constexpr uint16_t kTemperatureMeasurementStep = 100; /* 1 degree Celsius */
	static constexpr uint16_t kHumidityMeasurementStep = 200; /* 2 % RH */

	static void UpdateTemperatureTimeoutCallback(k_timer *timer);
	static void UpdateHumidityTimeoutCallback(k_timer *timer);
	static void FactoryResetTimerCallback(k_timer *timer);

	static void ButtonEventHandler(Nrf::ButtonState state, Nrf::ButtonMask hasChanged);

	int16_t mTemperatureSensorMaxValue = 0;
	int16_t mTemperatureSensorMinValue = 0;
	int16_t mCurrentTemperature = 0;

	uint16_t mHumiditySensorMaxValue = 0;
	uint16_t mHumiditySensorMinValue = 0;
	uint16_t mCurrentHumidity = 0;

	k_timer mHumidityTimer;
};
