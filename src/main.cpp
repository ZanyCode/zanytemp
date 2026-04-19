/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* Blinky debug stub — comment out the Matter app below to use this */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/devicetree.h>

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#include "app_task.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, CONFIG_CHIP_APP_LOG_LEVEL);

int main()
{
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	for(int i = 0; i < 5; i++) {
		gpio_pin_toggle_dt(&led);
		printk("hehe");
		k_msleep(500);
	}

	CHIP_ERROR err = AppTask::Instance().StartApp();

	LOG_ERR("Exited with code %" CHIP_ERROR_FORMAT, err.Format());
	return err == CHIP_NO_ERROR ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int rf_switch_select_ceramic(void)
{
	static const struct gpio_dt_spec rfswctl = GPIO_DT_SPEC_GET(DT_NODELABEL(rfsw_ctl), enable_gpios); // RF switch control pin GPIO2.5 (0 = onboard antenna)
	static const struct device *const rfsw_reg = DEVICE_DT_GET(DT_NODELABEL(rfsw_pwr)); // RF switch power pin GPIO2.3 (1 = VDD to the switch)

	/* RF switch for onboard antenna requires power to be enabled */
	regulator_enable(rfsw_reg);         	// on board rfsw power: ON
	return gpio_pin_set_dt(&rfswctl, 0);		    // set antenna switch position to onboard antenna
}
SYS_INIT(rf_switch_select_ceramic, APPLICATION, 0);