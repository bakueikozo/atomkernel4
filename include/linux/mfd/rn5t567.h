/*
 * MFD core driver for Ricoh RN5T567 PMIC
 *
 * Copyright (C) 2014 Beniamino Galvani <b.galvani@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LINUX_MFD_RN5T567_H
#define __LINUX_MFD_RN5T567_H

#include <linux/regmap.h>

#define RN5T567_LSIVER			0x00
#define RN5T567_OTPVER			0x01
#define RN5T567_IODAC			0x02
#define RN5T567_VINDAC			0x03
#define RN5T567_OUT32KEN		0x05
#define RN5T567_CPUCNT			0x06
#define RN5T567_PSWR			0x07
#define RN5T567_PONHIS			0x09
#define RN5T567_POFFHIS			0x0a
#define RN5T567_WATCHDOG		0x0b
#define RN5T567_WATCHDOGCNT		0x0c
#define RN5T567_PWRFUNC			0x0d
#define RN5T567_SLPCNT			0x0e
#define RN5T567_REPCNT			0x0f
#define RN5T567_PWRONTIMSET		0x10
#define RN5T567_NOETIMSETCNT		0x11
#define RN5T567_PWRIREN			0x12
#define RN5T567_PWRIRQ			0x13
#define RN5T567_PWRMON			0x14
#define RN5T567_PWRIRSEL		0x15
#define RN5T567_DC1_SLOT		0x16
#define RN5T567_DC2_SLOT		0x17
#define RN5T567_DC3_SLOT		0x18
#define RN5T567_DC4_SLOT		0x19
#define RN5T567_LDO1_SLOT		0x1b
#define RN5T567_LDO2_SLOT		0x1c
#define RN5T567_LDO3_SLOT		0x1d
#define RN5T567_LDO4_SLOT		0x1e
#define RN5T567_LDO5_SLOT		0x1f
#define RN5T567_PSO0_SLOT		0x25
#define RN5T567_PSO1_SLOT		0x26
#define RN5T567_PSO2_SLOT		0x27
#define RN5T567_PSO3_SLOT		0x28
#define RN5T567_LDORTC1_SLOT		0x2a
#define RN5T567_DC1CTL			0x2c
#define RN5T567_DC1CTL2			0x2d
#define RN5T567_DC2CTL			0x2e
#define RN5T567_DC2CTL2			0x2f
#define RN5T567_DC3CTL			0x30
#define RN5T567_DC3CTL2			0x31
#define RN5T567_DC4CTL			0x32
#define RN5T567_DC4CTL2			0x33
#define RN5T567_DC1DAC			0x36
#define RN5T567_DC2DAC			0x37
#define RN5T567_DC3DAC			0x38
#define RN5T567_DC4DAC			0x39
#define RN5T567_DC1DAC_SLP		0x3b
#define RN5T567_DC2DAC_SLP		0x3c
#define RN5T567_DC3DAC_SLP		0x3d
#define RN5T567_DC4DAC_SLP		0x3e
#define RN5T567_DCIREN			0x40
#define RN5T567_DCIRQ			0x41
#define RN5T567_DCIRMON			0x42
#define RN5T567_LDOEN1			0x44
#define RN5T567_LDOEN2			0x45
#define RN5T567_LDODIS			0x46
#define RN5T567_LDO1DAC			0x4c
#define RN5T567_LDO2DAC			0x4d
#define RN5T567_LDO3DAC			0x4e
#define RN5T567_LDO4DAC			0x4f
#define RN5T567_LDO5DAC			0x50
#define RN5T567_LDORTCDAC		0x56
#define RN5T567_LDORTC2DAC		0x57
#define RN5T567_LDO1DAC_SLP		0x58
#define RN5T567_LDO2DAC_SLP		0x59
#define RN5T567_LDO3DAC_SLP		0x5a
#define RN5T567_LDO4DAC_SLP		0x5b
#define RN5T567_LDO5DAC_SLP		0x5c
#define RN5T567_IOSEL			0x90
#define RN5T567_IOOUT			0x91
#define RN5T567_GPEDGE1			0x92
#define RN5T567_EN_GPIR			0x94
#define RN5T567_IR_GPR			0x95
#define RN5T567_IR_GPF			0x96
#define RN5T567_MON_IOIN		0x97
#define RN5T567_GPLED_FUNC		0x98
#define RN5T567_INTPOL			0x9c
#define RN5T567_INTEN			0x9d
#define RN5T567_INTMON			0x9e
#define RN5T567_PREVINDAC		0xb0
#define RN5T567_DIESET			0xbc
#define RN5T567_MAX_REG			RN5T567_DIESET
#define RN5T567_REG_NUM			((100) - (19))

#define RN5T567_REPCNT_REPWRON		BIT(0)
#define RN5T567_SLPCNT_SWPWROFF		BIT(0)

enum {
	RN5T567_DCDC1 = 0,
	RN5T567_DCDC2,
	RN5T567_DCDC3,
	RN5T567_DCDC4,
	RN5T567_LDO1,
	RN5T567_LDO2,
	RN5T567_LDO3,
	RN5T567_LDO4,
	RN5T567_LDO5,
	RN5T567_LDORTC1,
	//RN5T567_LDORTC2,
	RN5T567_REGLATOR_NUM,
};

enum {
	RN5T567_IRQ_SYSTEM = 0,
	RN5T567_IRQ_DCDC,
	RN5T567_IRQ_REV0,
	RN5T567_IRQ_REV1,
	RN5T567_IRQ_GPIO,
	RN5T567_IRQ_WDG,
	RN5T567_IRQ_REV2,
	RN5T567_IRQ_REV3,
	RN5T567_IRQ_NUM,
};

extern int rn5t567_irq_init(struct i2c_client *i2c, struct regmap *regmap);
extern void rn5t567_irq_deinit(void);
#endif /* __LINUX_MFD_RN5T567_H */
