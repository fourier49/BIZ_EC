/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * IR357x driver.
 */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ## args)

/* 8-bit I2C address */
#define IR357x_I2C_ADDR (0x8 << 1)

struct ir_setting {
	uint8_t reg;
	uint8_t value;
};

static struct ir_setting ir3570_settings[] = {
	{0x10, 0x22}, {0x11, 0x22}, {0x12, 0x88}, {0x13, 0x10},
	{0x14, 0x0d}, {0x15, 0x21}, {0x16, 0x21}, {0x17, 0x00},
	{0x18, 0x00}, {0x19, 0x00}, {0x1a, 0x00}, {0x1b, 0x00},
	{0x1c, 0x00}, {0x1d, 0x00}, {0x1e, 0x00}, {0x1f, 0x00},
	{0x20, 0x00}, {0x21, 0x00}, {0x22, 0x60}, {0x23, 0x60},
	{0x24, 0x74}, {0x25, 0x4e}, {0x26, 0xff}, {0x27, 0x80},
	{0x28, 0x00}, {0x29, 0x20}, {0x2a, 0x15}, {0x2b, 0x26},
	{0x2c, 0xb6}, {0x2d, 0x21}, {0x2e, 0x11}, {0x2f, 0x20},
	{0x30, 0xab}, {0x31, 0x14}, {0x32, 0x90}, {0x33, 0x4d},
	{0x34, 0x75}, {0x35, 0x64}, {0x36, 0x64}, {0x37, 0x09},
	{0x38, 0xc4}, {0x39, 0x20}, {0x3a, 0x80}, {0x3b, 0x00},
	{0x3c, 0x00}, {0x3d, 0xaa}, {0x3e, 0x00}, {0x3f, 0x05},
	{0x40, 0x50}, {0x41, 0x40}, {0x42, 0x00}, {0x43, 0x00},
	{0x44, 0x00}, {0x45, 0x00}, {0x46, 0x00}, {0x47, 0x00},
	{0x48, 0x1c}, {0x49, 0x0c}, {0x4a, 0x0f}, {0x4b, 0x40},
	{0x4c, 0x80}, {0x4d, 0x40}, {0x4e, 0x80},
	{0x51, 0x00}, {0x52, 0x45}, {0x53, 0x59},
	{0x54, 0x23}, {0x55, 0xae}, {0x56, 0x68}, {0x57, 0x24},
	{0x58, 0x62}, {0x59, 0x42}, {0x5a, 0x34}, {0x5b, 0x00},
	{0x5c, 0x30}, {0x5d, 0x05}, {0x5e, 0x02}, {0x5f, 0x35},
	{0x60, 0x30}, {0x61, 0x00}, {0x62, 0xd8}, {0x63, 0x00},
	{0x64, 0x52}, {0x65, 0x28}, {0x66, 0x14}, {0x67, 0x87},
	{0x68, 0x80}, {0x69, 0x00}, {0x6a, 0x00}, {0x6b, 0x00},
	{0x6c, 0x00}, {0x6d, 0xff}, {0x6e, 0x06}, {0x6f, 0xff},
	{0x70, 0xff}, {0x71, 0x20}, {0x72, 0x00}, {0x73, 0x01},
	{0x74, 0x00}, {0x75, 0x00}, {0x76, 0x00}, {0x77, 0x00},
	{0x78, 0x00}, {0x79, 0x00}, {0x7a, 0x00}, {0x7b, 0x00},
	{0x7c, 0x15}, {0x7d, 0x15}, {0x7e, 0x00}, {0x7f, 0x00},
	{0x80, 0x00}, {0x81, 0x00}, {0x82, 0x00}, {0x83, 0x00},
	{0x84, 0x00}, {0x85, 0x00}, {0x86, 0x00}, {0x87, 0x00},
	{0x88, 0x88}, {0x89, 0x88}, {0x8a, 0x01}, {0x8b, 0x42},
	{0x8d, 0x00}, {0x8e, 0x00}, {0x8f, 0x1f},
	{0, 0}
};

static struct ir_setting ir3571_settings[] = {
	{0x18, 0x22}, {0x19, 0x22}, {0x1a, 0x08}, {0x1b, 0x10},
	{0x1c, 0x06}, {0x1d, 0x21}, {0x1e, 0x21}, {0x1f, 0x83},
	{0x20, 0x83}, {0x21, 0x00}, {0x22, 0x00}, {0x23, 0x00},
	{0x24, 0x00}, {0x25, 0x00}, {0x26, 0x00}, {0x27, 0x34},
	{0x28, 0x34}, {0x29, 0x74}, {0x2a, 0x4e}, {0x2b, 0xff},
	{0x2c, 0x00}, {0x2d, 0x1d}, {0x2e, 0x14}, {0x2f, 0x1f},
	{0x30, 0x88}, {0x31, 0x9a}, {0x32, 0x1e}, {0x33, 0x19},
	{0x34, 0xe9}, {0x35, 0x40}, {0x36, 0x90}, {0x37, 0x6d},
	{0x38, 0x75}, {0x39, 0xa0}, {0x3a, 0x84}, {0x3b, 0x08},
	{0x3c, 0xc5}, {0x3d, 0xa0}, {0x3e, 0x80}, {0x3f, 0xaa},
	{0x40, 0x50}, {0x41, 0x4b}, {0x42, 0x02}, {0x43, 0x04},
	{0x44, 0x00}, {0x45, 0x00}, {0x46, 0x00}, {0x47, 0x78},
	{0x48, 0x56}, {0x49, 0x18}, {0x4a, 0x88}, {0x4b, 0x00},
	{0x4c, 0x80}, {0x4d, 0x60}, {0x4e, 0x60}, {0x4f, 0xff},
	{0x50, 0xff}, {0x51, 0x00}, {0x52, 0x9b}, {0x53, 0xaa},
	{0x54, 0xd8}, {0x55, 0x56}, {0x56, 0x31}, {0x57, 0x1a},
	{0x58, 0x12}, {0x59, 0x63}, {0x5a, 0x00}, {0x5b, 0x09},
	{0x5c, 0x02}, {0x5d, 0x00}, {0x5e, 0xea}, {0x5f, 0x00},
	{0x60, 0xb0}, {0x61, 0x1e}, {0x62, 0x00}, {0x63, 0x56},
	{0x64, 0x00}, {0x65, 0x00}, {0x66, 0x00}, {0x67, 0x00},
	{0x68, 0x28}, {0x69, 0x00}, {0x6a, 0x00}, {0x6b, 0x00},
	{0x6c, 0x00}, {0x6d, 0x00}, {0x6e, 0x00}, {0x6f, 0x00},
	{0x70, 0x80}, {0x71, 0x00}, {0x72, 0x00}, {0x73, 0x00},
	{0x74, 0x00}, {0x75, 0xbf}, {0x76, 0x06}, {0x77, 0xff},
	{0x78, 0xff}, {0x79, 0x04}, {0x7a, 0x00}, {0x7b, 0x1d},
	{0x7c, 0xa0}, {0x7d, 0x10}, {0x7e, 0x00}, {0x7f, 0x8a},
	{0x80, 0x1b}, {0x81, 0x11}, {0x82, 0x00}, {0x83, 0x00},
	{0x84, 0x00}, {0x85, 0x00}, {0x86, 0x00}, {0x87, 0x00},
	{0x88, 0x00}, {0x89, 0x00}, {0x8a, 0x00}, {0x8b, 0x00},
	{0x8c, 0x00}, {0x8d, 0x00}, {0x8e, 0x00}, {0x8f, 0x00},
	{0, 0}
};

static uint8_t ir357x_read(uint8_t reg)
{
	int res;
	int val;

	res = i2c_read8(I2C_PORT_REGULATOR, IR357x_I2C_ADDR, reg, &val);
	if (res)
		return 0xee;

	return val;
}

static void ir357x_write(uint8_t reg, uint8_t val)
{
	int res;

	res = i2c_write8(I2C_PORT_REGULATOR, IR357x_I2C_ADDR, reg, val);
	if (res)
		CPRINTF("IR I2C write failed\n");
}

static int ir357x_get_version(void)
{
	/* IR3571 on Link EVT */
	if ((ir357x_read(0xfc) == 'I') && (ir357x_read(0xfd) == 'R') &&
	    ((ir357x_read(0x0a) & 0xe) == 0))
		return 3571;

	/* IR3570A on Link Proto 0/1 and Link DVT */
	if ((ir357x_read(0x92) == 'C') && (ir357x_read(0xcd) == 0x24))
		return 3570;

	/* Unknown and unsupported chip */
	return -1;
}

struct ir_setting *ir357x_get_settings(void)
{
	int version = ir357x_get_version();

	if (version == 3570)
		return ir3570_settings;
	else if (version == 3571)
		return ir3571_settings;
	else
		return NULL;
}

static void ir357x_prog(void)
{
	struct ir_setting *settings = ir357x_get_settings();

	if (settings) {
		for (; settings->reg; settings++)
			ir357x_write(settings->reg, settings->value);
	} else {
		CPRINTF("IR%d chip unsupported. Skip writing settings!\n",
			ir357x_get_version());
		return;
	}

	CPRINTF("IR%d registers UPDATED\n", ir357x_get_version());
}

static void ir357x_dump(void)
{
	int i;

	for (i = 0; i < 256; i++) {
		if (!(i & 0xf)) {
			ccprintf("\n%02x: ", i);
			cflush();
		}
		ccprintf("%02x ", ir357x_read(i));
	}
	ccprintf("\n");
}

static int ir357x_check(void)
{
	uint8_t val;
	int diff = 0;
	struct ir_setting *settings = ir357x_get_settings();

	if (!settings) {
		ccprintf("no setting for chip IR%d !\n", ir357x_get_version());
		return 1;
	}

	for (; settings->reg; settings++) {
		val = ir357x_read(settings->reg);
		if (val != settings->value) {
			ccprintf("DIFF reg 0x%02x %02x->%02x\n",
				 settings->reg, settings->value, val);
			cflush();
			diff++;
		}
	}
	return !!diff;
}

#ifdef CONFIG_CMD_REGULATOR
static int command_ir357x(int argc, char **argv)
{
	int reg, val;
	char *rem;

	if (1 == argc) { /* dump all registers */
		ir357x_dump();
		return EC_SUCCESS;
	} else if (2 == argc) {
		if (!strcasecmp(argv[1], "check")) {
			ir357x_check();
		} else { /* read one register */
			reg = strtoi(argv[1], &rem, 16);
			if (*rem) {
				ccprintf("Invalid register: %s\n", argv[1]);
				return EC_ERROR_INVAL;
			}
			ccprintf("reg 0x%02x = 0x%02x\n", reg,
				 ir357x_read(reg));
		}
		return EC_SUCCESS;
	} else if (3 == argc) { /* write one register */
		reg = strtoi(argv[1], &rem, 16);
		if (*rem) {
			ccprintf("Invalid register: %s\n", argv[1]);
			return EC_ERROR_INVAL;
		}
		val = strtoi(argv[2], &rem, 16);
		if (*rem) {
			ccprintf("Invalid value: %s\n", argv[2]);
			return EC_ERROR_INVAL;
		}
		ir357x_write(reg, val);
		return EC_SUCCESS;
	}

	return EC_ERROR_INVAL;
}
DECLARE_CONSOLE_COMMAND(ir357x, command_ir357x,
			"[check|write]",
			"IR357x core regulator control",
			NULL);
#endif

static void ir357x_hot_settings(void)
{
	/* dynamically apply settings to workaround issue */
	ir357x_prog();
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, ir357x_hot_settings, HOOK_PRIO_DEFAULT);
