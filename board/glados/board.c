/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Glados board-specific configuration */

#include "button.h"
#include "charger.h"
#include "extpower.h"
#include "gpio.h"
#include "i2c.h"
#include "lid_switch.h"
#include "motion_sense.h"
#include "power.h"
#include "power_button.h"
#include "switch.h"
#include "util.h"

#define GPIO_KB_INPUT (GPIO_INPUT | GPIO_PULL_UP)
#define GPIO_KB_OUTPUT (GPIO_ODR_HIGH)

/* Exchange status with PD MCU. */
static void pd_mcu_interrupt(enum gpio_signal signal)
{
}

void vbus0_evt(enum gpio_signal signal)
{
}

void vbus1_evt(enum gpio_signal signal)
{
}

void usb0_evt(enum gpio_signal signal)
{
}

void usb1_evt(enum gpio_signal signal)
{
}

#include "gpio_list.h"

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_RSMRST_L_PGOOD,    1, "RSMRST_N_PWRGD"},
	{GPIO_PCH_SLP_S0_L,      1, "SLP_S0_DEASSERTED"},
	{GPIO_PCH_SLP_S3_L,      1, "SLP_S3_DEASSERTED"},
	{GPIO_PCH_SLP_S4_L,      1, "SLP_S4_DEASSERTED"},
	{GPIO_PCH_SLP_SUS_L,     1, "SLP_SUS_DEASSERTED"},
	{GPIO_PMIC_DPWROK,       1, "PMIC_DPWROK"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

const struct i2c_port_t i2c_ports[]  = {
	{"batt",     MEC1322_I2C0_0, 100,  GPIO_I2C0_0_SCL, GPIO_I2C0_0_SDA},
	{"muxes",    MEC1322_I2C0_0, 100,  GPIO_I2C0_1_SCL, GPIO_I2C0_1_SDA},
	{"pd_mcu",   MEC1322_I2C1,  1000,  GPIO_I2C1_SCL,   GPIO_I2C1_SDA},
	{"sensors",  MEC1322_I2C2,   400,  GPIO_I2C2_SCL,   GPIO_I2C2_SDA  },
	{"pmic",     MEC1322_I2C3,   400,  GPIO_I2C3_SCL,   GPIO_I2C3_SDA  },
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/**
 * Discharge battery when on AC power for factory test.
 */
int board_discharge_on_ac(int enable)
{
	return charger_discharge_on_ac(enable);
}

struct motion_sensor_t motion_sensors[] = {

};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

const struct button_config buttons[CONFIG_BUTTON_COUNT] = {
	{ 0 },
	{ 0 },
};