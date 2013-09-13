/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery driver for BQ27541.
 */

#include "battery.h"
#include "console.h"
#include "i2c.h"
#include "util.h"

#define BQ27541_ADDR                0xaa
#define BQ27541_TYPE_ID             0x0541

#define REG_CTRL                    0x00
#define REG_AT_RATE                 0x02
#define REG_AT_RATE_TIME_TO_EMPTY   0x04
#define REG_TEMPERATURE             0x06
#define REG_VOLTAGE                 0x08
#define REG_FLAGS                   0x0a
#define REG_NOMINAL_CAPACITY        0x0c
#define REG_FULL_AVAILABLE_CAPACITY 0x0e
#define REG_REMAINING_CAPACITY      0x10
#define REG_FULL_CHARGE_CAPACITY    0x12
#define REG_AVERAGE_CURRENT         0x14
#define REG_TIME_TO_EMPTY           0x16
#define REG_TIME_TO_FULL            0x18
#define REG_STANDBY_CURRENT         0x1a
#define REG_STANDBY_TIME_TO_EMPTY   0x1c
#define REG_MAX_LOAD_CURRENT        0x1e
#define REG_MAX_LOAD_TIME_TO_EMPTY  0x20
#define REG_AVAILABLE_ENERGY        0x22
#define REG_AVERAGE_POEWR           0x24
#define REG_TT_EAT_CONSTANT_POWER   0x26
#define REG_CYCLE_COUNT             0x2a
#define REG_STATE_OF_CHARGE         0x2c
#define REG_DESIGN_CAPACITY         0x3c
#define REG_DEVICE_NAME_LENGTH      0x62
#define MAX_DEVICE_NAME_LENGTH      7
#define REG_DEVICE_NAME             0x63

static int bq27541_read(int offset, int *data)
{
	return i2c_read16(I2C_PORT_HOST, BQ27541_ADDR, offset, data);
}

static int bq27541_read8(int offset, int *data)
{
	return i2c_read8(I2C_PORT_HOST, BQ27541_ADDR, offset, data);
}

static int bq27541_write(int offset, int data)
{
	return i2c_write16(I2C_PORT_HOST, BQ27541_ADDR, offset, data);
}

int bq27541_probe(void)
{
	int rv;
	int dev_type;

	rv = bq27541_write(REG_CTRL, 0x1);
	rv |= bq27541_read(REG_CTRL, &dev_type);

	if (rv)
		return rv;
	return (dev_type == BQ27541_TYPE_ID) ? EC_SUCCESS : EC_ERROR_UNKNOWN;
}

int battery_device_name(char *device_name, int buf_size)
{
	int rv, i, val;
	int len = MIN(7, buf_size - 1);

	rv = bq27541_read8(REG_DEVICE_NAME_LENGTH, &val);
	if (rv)
		return rv;
	len = MIN(len, val);

	for (i = 0; i < len; ++i) {
		rv |= bq27541_read8(REG_DEVICE_NAME + i, &val);
		device_name[i] = val;
	}
	device_name[i] = '\0';

	return rv;
}

int battery_temperature(int *deci_kelvin)
{
	return bq27541_read(REG_TEMPERATURE, deci_kelvin);
}

int battery_voltage(int *voltage)
{
	return bq27541_read(REG_VOLTAGE, voltage);
}

int battery_state_of_charge(int *percent)
{
	return bq27541_read(REG_STATE_OF_CHARGE, percent);
}

int battery_state_of_charge_abs(int *percent)
{
	return battery_state_of_charge(percent);
}

int battery_remaining_capacity(int *capacity)
{
	return bq27541_read(REG_REMAINING_CAPACITY, capacity);
}

int battery_full_charge_capacity(int *capacity)
{
	return bq27541_read(REG_FULL_CHARGE_CAPACITY, capacity);
}

int battery_time_to_empty(int *minutes)
{
	return bq27541_read(REG_TIME_TO_EMPTY, minutes);
}

int battery_time_to_full(int *minutes)
{
	return bq27541_read(REG_TIME_TO_FULL, minutes);
}

int battery_cycle_count(int *count)
{
	return bq27541_read(REG_CYCLE_COUNT, count);
}

int battery_design_capacity(int *capacity)
{
	return bq27541_read(REG_DESIGN_CAPACITY, capacity);
}

int battery_average_current(int *current)
{
	int rv = bq27541_read(REG_AVERAGE_CURRENT, current);
	*current = (int)((int16_t)*current);
	return rv;
}

int battery_current(int *current)
{
	return battery_average_current(current);
}

int battery_time_at_rate(int rate, int *minutes)
{
	int rv;

	rv = bq27541_write(REG_AT_RATE, rate);
	if (rv)
		return rv;
	return bq27541_read(REG_AT_RATE_TIME_TO_EMPTY, minutes);
}

int battery_manufacturer_name(char *name, int buf_size)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int battery_device_chemistry(char *chemistry, int buf_size)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int battery_serial_number(int *serial)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int battery_desired_voltage(int *voltage)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int battery_design_voltage(int *voltage)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int battery_charging_allowed(int *allowed)
{
	int rv, val;

	rv = bq27541_read(REG_FLAGS, &val);
	if (rv)
		return rv;
	*allowed = (val & 0x100);
	return EC_SUCCESS;
}

int battery_desired_current(int *current)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int battery_get_battery_mode(int *mode)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int battery_is_in_10mw_mode(int *val)
{
	/* Always using mAh unit */
	*val = 0;
	return EC_SUCCESS;
}

int battery_set_10mw_mode(int enabled)
{
	/* Not supported by this battery chip */
	return EC_ERROR_INVAL;
}