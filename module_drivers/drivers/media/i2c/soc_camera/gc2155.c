/*
 * gc2155 Camera Driver
 *
 * Copyright (C) 2010 Alberto Panizzo <maramaopercheseimorto@gmail.com>
 *
 * Based on ov772x, ov9640 drivers and previous non merged implementations.
 *
 * Copyright 2005-2009 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2006, OmniVision
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <media/soc_camera.h>
#include <media/v4l2-clk.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-image-sizes.h>

#include <linux/clk.h>
#include <linux/regulator/consumer.h>

#define REG_CHIP_ID_HIGH        0xf0
#define REG_CHIP_ID_LOW         0xf1

#define CHIP_ID_HIGH            0x21
#define CHIP_ID_LOW				0x55

/* Private v4l2 controls */
#define V4L2_CID_PRIVATE_BALANCE  (V4L2_CID_PRIVATE_BASE + 0)
#define V4L2_CID_PRIVATE_EFFECT  (V4L2_CID_PRIVATE_BASE + 1)

#define REG14				0x14
#define REG14_HFLIP_IMG		0x01 /* Horizontal mirror image ON/OFF */
#define REG14_VFLIP_IMG     0x02 /* Vertical flip image ON/OFF */

/* whether sensor support high resolution (> vga) preview or not */
#define SUPPORT_HIGH_RESOLUTION_PRE		1

/*
 * Struct
 */
struct regval_list {
	u8 reg_num;
	u8 value;
};

struct mode_list {
	u8 index;
	const struct regval_list *mode_regs;
};

/* Supported resolutions */
enum gc2155_width {
	W_QVGA	= 320,
	W_VGA	= 640,
	W_720P  = 1280,
};

enum gc2155_height {
	H_QVGA	= 240,
	H_VGA	= 480,
	H_720P  = 720,
};

struct gc2155_win_size {
	char *name;
	enum gc2155_width width;
	enum gc2155_height height;
	const struct regval_list *regs;
};

struct gc2155_priv {
	struct v4l2_subdev		subdev;
	struct v4l2_ctrl_handler	hdl;
	u32	cfmt_code;
	struct v4l2_clk			*clk;
	const struct gc2155_win_size	*win;

	int				model;
	u16				balance_value;
	u16				effect_value;
	u16				flag_vflip:1;
	u16				flag_hflip:1;

	struct soc_camera_subdev_desc	ssdd_dt;
	struct gpio_desc *resetb_gpio;
	struct gpio_desc *pwdn_gpio;
	struct gpio_desc *vcc_en_gpio;

	struct regulator *reg;
	struct regulator *reg1;
};

static int gc2155_s_power(struct v4l2_subdev *sd, int on);
static int gc2155_set_params(struct i2c_client *client, u32 *width, u32 *height, u32 code);

static inline int gc2155_write_reg(struct i2c_client * client, unsigned char addr, unsigned char value)
{
	return i2c_smbus_write_byte_data(client, addr, value);
}
static inline char gc2155_read_reg(struct i2c_client *client, unsigned char addr)
{
	char ret;
	ret = i2c_smbus_read_byte_data(client, addr);
	return ret;
}
/*
 * Registers settings
 */

#define ENDMARKER { 0xff, 0xff }

static const struct regval_list gc2155_init_regs[] = {
	{0xfe,0xf0},
	{0xfe,0xf0},
	{0xfe,0xf0},
	{0xfc,0x06},
	{0xf6,0x00},
	{0xf7,0x1d},
	{0xf8,0x84},
	{0xfa,0x00},
	{0xf9,0xfe},
	{0xf2,0x00},
	{0xfe,0x00},
	{0x03,0x04},
	{0x04,0xe2},
	{0x09,0x00},
	{0x0a,0x00},
	{0x0b,0x00},
	{0x0c,0x00},
	{0x0d,0x04},
	{0x0e,0xc0},
	{0x0f,0x06},
	{0x10,0x50},
	{0x12,0x2e},
	{0x17,0x14}, // mirror
	{0x18,0x02},
	{0x19,0x0e},
	{0x1a,0x01},
	{0x1b,0x4b},
	{0x1c,0x07},
	{0x1d,0x10},
	{0x1e,0x98},
	{0x1f,0x78},
	{0x20,0x05},
	{0x21,0x40},
	{0x22,0xf0},
	{0x24,0x16},
	{0x25,0x01},
	{0x26,0x10},
	{0x2d,0x40},
	{0x30,0x01},
	{0x31,0x90},
	{0x33,0x04},
	{0x34,0x01},
	{0xfe,0x00},
	{0x80,0xff},
	{0x81,0x2c},
	{0x82,0xfa},
	{0x83,0x00},
	{0x84,0x02}, //y u yv
	{0x85,0x08},
	{0x86,0x02},
	{0x89,0x03},
	{0x8a,0x00},
	{0x8b,0x00},
	{0xb0,0x55},
	{0xc3,0x11}, //00
	{0xc4,0x20},
	{0xc5,0x30},
	{0xc6,0x38},
	{0xc7,0x40},
	{0xec,0x02},
	{0xed,0x04},
	{0xee,0x60},
	{0xef,0x90},
	{0xb6,0x01},
	{0x90,0x01},
	{0x91,0x00},
	{0x92,0x00},
	{0x93,0x00},
	{0x94,0x00},
	{0x95,0x04},
	{0x96,0xb0},
	{0x97,0x06},
	{0x98,0x40},
	{0xfe,0x00},
	{0x18,0x02},
	{0x40,0x42},
	{0x41,0x00},
	{0x43,0x5b},//0X54
	{0x5e,0x00},
	{0x5f,0x00},
	{0x60,0x00},
	{0x61,0x00},
	{0x62,0x00},
	{0x63,0x00},
	{0x64,0x00},
	{0x65,0x00},
	{0x66,0x20},
	{0x67,0x20},
	{0x68,0x20},
	{0x69,0x20},
	{0x6a,0x08},
	{0x6b,0x08},
	{0x6c,0x08},
	{0x6d,0x08},
	{0x6e,0x08},
	{0x6f,0x08},
	{0x70,0x08},
	{0x71,0x08},
	{0x72,0xf0},
	{0x7e,0x3c},
	{0x7f,0x00},
	{0xfe,0x00},
	{0xfe,0x01},
	{0x01,0x08},
	{0x02,0xc0},
	{0x03,0x04},
	{0x04,0x90},
	{0x05,0x30},
	{0x06,0x98},
	{0x07,0x28},
	{0x08,0x6c},
	{0x09,0x00},
	{0x0a,0xc2},
	{0x0b,0x11},
	{0x0c,0x10},
	{0x13,0x2d},
	{0x17,0x00},
	{0x1c,0x11},
	{0x1e,0x61},
	{0x1f,0x30},
	{0x20,0x40},
	{0x22,0x80},
	{0x23,0x20},
	{0x12,0x35},
	{0x15,0x50},
	{0x10,0x31},
	{0x3e,0x28},
	{0x3f,0xe0},
	{0x40,0xe0},
	{0x41,0x08},
	{0xfe,0x02},
	{0x0f,0x05},
	{0xfe,0x02},
	{0x90,0x6c},
	{0x91,0x03},
	{0x92,0xc4},
	{0x97,0x64},
	{0x98,0x88},
	{0x9d,0x08},
	{0xa2,0x11},
	{0xfe,0x00},
	{0xfe,0x02},
	{0x80,0xc1},
	{0x81,0x08},
	{0x82,0x05},
	{0x83,0x04},
	{0x86,0x80},
	{0x87,0x30},
	{0x88,0x15},
	{0x89,0x80},
	{0x8a,0x60},
	{0x8b,0x30},
	{0xfe,0x01},
	{0x21,0x14},
	{0xfe,0x02},
	{0x3c,0x06},
	{0x3d,0x40},
	{0x48,0x30},
	{0x49,0x06},
	{0x4b,0x08},
	{0x4c,0x20},
	{0xa3,0x50},
	{0xa4,0x30},
	{0xa5,0x40},
	{0xa6,0x80},
	{0xab,0x40},
	{0xae,0x0c},
	{0xb3,0x42},
	{0xb4,0x24},
	{0xb6,0x50},
	{0xb7,0x01},
	{0xb9,0x28},
	{0xfe,0x00},
	{0xfe,0x02},
	{0x10,0x0d},
	{0x11,0x12},
	{0x12,0x17},
	{0x13,0x1c},
	{0x14,0x27},
	{0x15,0x34},
	{0x16,0x44},
	{0x17,0x55},
	{0x18,0x6e},
	{0x19,0x81},
	{0x1a,0x91},
	{0x1b,0x9c},
	{0x1c,0xaa},
	{0x1d,0xbb},
	{0x1e,0xca},
	{0x1f,0xd5},
	{0x20,0xe0},
	{0x21,0xe7},
	{0x22,0xed},
	{0x23,0xf6},
	{0x24,0xfb},
	{0x25,0xff},
	{0xfe,0x02},
	{0x26,0x0d},
	{0x27,0x12},
	{0x28,0x17},
	{0x29,0x1c},
	{0x2a,0x27},
	{0x2b,0x34},
	{0x2c,0x44},
	{0x2d,0x55},
	{0x2e,0x6e},
	{0x2f,0x81},
	{0x30,0x91},
	{0x31,0x9c},
	{0x32,0xaa},
	{0x33,0xbb},
	{0x34,0xca},
	{0x35,0xd5},
	{0x36,0xe0},
	{0x37,0xe7},
	{0x38,0xed},
	{0x39,0xf6},
	{0x3a,0xfb},
	{0x3b,0xff},
	{0xfe,0x02},
	{0xd1,0x28},
	{0xd2,0x28},
	{0xdd,0x14},
	{0xde,0x88},
	{0xed,0x80},
	{0xfe,0x01},
	{0xc2,0x1f},
	{0xc3,0x13},
	{0xc4,0x0e},
	{0xc8,0x16},
	{0xc9,0x0f},
	{0xca,0x0c},
	{0xbc,0x52},
	{0xbd,0x2c},
	{0xbe,0x27},
	{0xb6,0x47},
	{0xb7,0x32},
	{0xb8,0x30},
	{0xc5,0x00},
	{0xc6,0x00},
	{0xc7,0x00},
	{0xcb,0x00},
	{0xcc,0x00},
	{0xcd,0x00},
	{0xbf,0x0e},
	{0xc0,0x00},
	{0xc1,0x00},
	{0xb9,0x08},
	{0xba,0x00},
	{0xbb,0x00},
	{0xaa,0x0a},
	{0xab,0x0c},
	{0xac,0x0d},
	{0xad,0x02},
	{0xae,0x06},
	{0xaf,0x05},
	{0xb0,0x00},
	{0xb1,0x05},
	{0xb2,0x02},
	{0xb3,0x04},
	{0xb4,0x04},
	{0xb5,0x05},
	{0xd0,0x00},
	{0xd1,0x00},
	{0xd2,0x00},
	{0xd6,0x02},
	{0xd7,0x00},
	{0xd8,0x00},
	{0xd9,0x00},
	{0xda,0x00},
	{0xdb,0x00},
	{0xd3,0x00},
	{0xd4,0x00},
	{0xd5,0x00},
	{0xa4,0x04},
	{0xa5,0x00},
	{0xa6,0x77},
	{0xa7,0x77},
	{0xa8,0x77},
	{0xa9,0x77},
	{0xa1,0x80},
	{0xa2,0x80},
	{0xfe,0x01},
	{0xdc,0x35},
	{0xdd,0x28},
	{0xdf,0x0d},
	{0xe0,0x70},
	{0xe1,0x78},
	{0xe2,0x70},
	{0xe3,0x78},
	{0xe6,0x90},
	{0xe7,0x70},
	{0xe8,0x90},
	{0xe9,0x70},
	{0xfe,0x00},
	{0xfe,0x01},
	{0x4f,0x00},
	{0x4f,0x00},
	{0x4b,0x01},
	{0x4f,0x00},
	{0x4c,0x01},
	{0x4d,0x71},
	{0x4e,0x02},
	{0x4c,0x01},
	{0x4d,0x91},
	{0x4e,0x02},
	{0x4c,0x01},
	{0x4d,0x50},
	{0x4e,0x02},
	{0x4c,0x01},
	{0x4d,0x70},
	{0x4e,0x02},
	{0x4c,0x01},
	{0x4d,0x90},
	{0x4e,0x02},
	{0x4c,0x01},
	{0x4d,0xb0},
	{0x4e,0x02},
	{0x4c,0x01},
	{0x4d,0xd0},
	{0x4e,0x02},
	{0x4c,0x01},
	{0x4d,0x4f},
	{0x4e,0x02},
	{0x4c,0x01},
	{0x4d,0x6f},
	{0x4e,0x02},
	{0x4c,0x01},
	{0x4d,0x8f},
	{0x4e,0x02},
	{0x4c,0x01},
	{0x4d,0xaf},
	{0x4e,0x02},
	{0x4c,0x01},
	{0x4d,0xcf},
	{0x4e,0x02},
	{0x4c,0x01},
	{0x4d,0x6e},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0x8e},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0xae},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0xce},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0x4d},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0x6d},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0x8d},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0xad},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0xcd},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0x4c},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0x6c},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0x8c},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0xac},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0xcc},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0xec},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0x4b},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0x6b},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0x8b},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0xab},
	{0x4e,0x03},
	{0x4c,0x01},
	{0x4d,0x8a},
	{0x4e,0x04},
	{0x4c,0x01},
	{0x4d,0xaa},
	{0x4e,0x04},
	{0x4c,0x01},
	{0x4d,0xca},
	{0x4e,0x04},
	{0x4c,0x01},
	{0x4d,0xa9},
	{0x4e,0x04},
	{0x4c,0x01},
	{0x4d,0xc9},
	{0x4e,0x04},
	{0x4c,0x01},
	{0x4d,0xcb},
	{0x4e,0x05},
	{0x4c,0x01},
	{0x4d,0xeb},
	{0x4e,0x05},
	{0x4c,0x02},
	{0x4d,0x0b},
	{0x4e,0x05},
	{0x4c,0x02},
	{0x4d,0x2b},
	{0x4e,0x05},
	{0x4c,0x02},
	{0x4d,0x4b},
	{0x4e,0x05},
	{0x4c,0x01},
	{0x4d,0xea},
	{0x4e,0x05},
	{0x4c,0x02},
	{0x4d,0x0a},
	{0x4e,0x05},
	{0x4c,0x02},
	{0x4d,0x2a},
	{0x4e,0x05},
	{0x4c,0x02},
	{0x4d,0x6a},
	{0x4e,0x06},
	{0x4c,0x02},
	{0x4d,0x29},
	{0x4e,0x06},
	{0x4c,0x02},
	{0x4d,0x49},
	{0x4e,0x06},
	{0x4c,0x02},
	{0x4d,0x69},
	{0x4e,0x06},
	{0x4c,0x02},
	{0x4d,0x89},
	{0x4e,0x06},
	{0x4c,0x02},
	{0x4d,0xa9},
	{0x4e,0x06},
	{0x4c,0x02},
	{0x4d,0xc9},
	{0x4e,0x06},
	{0x4c,0x02},
	{0x4d,0x48},
	{0x4e,0x06},
	{0x4c,0x02},
	{0x4d,0x68},
	{0x4e,0x06},
	{0x4c,0x03},
	{0x4d,0x09},
	{0x4e,0x07},
	{0x4c,0x02},
	{0x4d,0xa8},
	{0x4e,0x07},
	{0x4c,0x02},
	{0x4d,0xc8},
	{0x4e,0x07},
	{0x4c,0x02},
	{0x4d,0xe8},
	{0x4e,0x07},
	{0x4c,0x03},
	{0x4d,0x08},
	{0x4e,0x07},
	{0x4c,0x03},
	{0x4d,0x28},
	{0x4e,0x07},
	{0x4c,0x02},
	{0x4d,0x87},
	{0x4e,0x07},
	{0x4c,0x02},
	{0x4d,0xa7},
	{0x4e,0x07},
	{0x4c,0x02},
	{0x4d,0xc7},
	{0x4e,0x07},
	{0x4c,0x02},
	{0x4d,0xe7},
	{0x4e,0x07},
	{0x4c,0x03},
	{0x4d,0x07},
	{0x4e,0x07},
	{0x4f,0x01},
	{0xfe,0x01},
	{0x50,0x80},
	{0x51,0xa8},
	{0x52,0x57},
	{0x53,0x38},
	{0x54,0xc7},
	{0x56,0x0e},
	{0x58,0x08},
	{0x5b,0x00},
	{0x5c,0x74},
	{0x5d,0x8b},
	{0x61,0xd3},
	{0x62,0x90},
	{0x63,0xaa},
	{0x65,0x04},
	{0x67,0xb2},
	{0x68,0xac},
	{0x69,0x00},
	{0x6a,0xb2},
	{0x6b,0xac},
	{0x6c,0xdc},
	{0x6d,0xb0},
	{0x6e,0x30},
	{0x6f,0x40},
	{0x70,0x05},
	{0x71,0x80},
	{0x72,0x80},
	{0x73,0x30},
	{0x74,0x01},
	{0x75,0x01},
	{0x7f,0x08},
	{0x76,0x70},
	{0x77,0x48},
	{0x78,0xa0},
	{0xfe,0x00},
	{0xfe,0x02},
	{0xc0,0x01},
	{0xc1,0x4a},
	{0xc2,0xf3},
	{0xc3,0xfc},
	{0xc4,0xe4},
	{0xc5,0x48},
	{0xc6,0xec},
	{0xc7,0x45},
	{0xc8,0xf8},
	{0xc9,0x02},
	{0xca,0xfe},
	{0xcb,0x42},
	{0xcc,0x00},
	{0xcd,0x45},
	{0xce,0xf0},
	{0xcf,0x00},
	{0xe3,0xf0},
	{0xe4,0x45},
	{0xe5,0xe8},
	{0xfe,0x01},
	{0x9f,0x42},
	{0xfe,0x00},
	{0xfe,0x00},
	{0xf2,0x0f},
	{0xfe,0x00},
	{0x05,0x01},
	{0x06,0x56},
	{0x07,0x00},
	{0x08,0x32},
	{0xfe,0x01},
	{0x25,0x00},
	{0x26,0xfa},
	{0x27,0x04},
	{0x28,0xe2}, //20fps
	{0x29,0x06},
	{0x2a,0xd6}, //16fps
	{0x2b,0x07},
	{0x2c,0xd0}, //12fps
	{0x2d,0x0b},
	{0x2e,0xb8}, //8fps
	{0xfe,0x00},
	{0xfe,0x00},
	{0xfa,0x00},
	{0xfd,0x01},
	{0xfe,0x00},
	{0x90,0x01},
	{0x91,0x00},
	{0x92,0x00},
	{0x93,0x00},
	{0x94,0x00},
	{0x95,0x04},// win_size 320 * 240
	{0x96,0xb0},
	{0x97,0x06},
	{0x98,0x40},


	{0x99,0x11},
	{0x9a,0x06},
	{0xfe,0x01},
	{0xec,0x01},
	{0xed,0x02},
	{0xee,0x30},
	{0xef,0x48},
	{0xfe,0x01},
	{0x74,0x00},
	{0xfe,0x01},
	{0x01,0x04},
	{0x02,0x60},
	{0x03,0x02},
	{0x04,0x48},
	{0x05,0x18},
	{0x06,0x4c},
	{0x07,0x14},
	{0x08,0x36},
	{0x0a,0xc0},
	{0x21,0x14},
	{0xfe,0x00},
	{0xfe,0x00},
	{0xc3,0x11},
	{0xc4,0x20},
	{0xc5,0x30},
	{0xfa,0x11},//pclk rate
	{0x86,0x06},//pclk polar
	{0xfe,0x00},
	ENDMARKER,
};

/* qvga Center crop */
static const struct regval_list gc2155_qvga_regs[] = {
	{0xfe,0x00},
	{0xfd,0x01},
	// crop window
	{0xfe,0x00},
	{0x90,0x01},
	{0x91,0x00},
	{0x92,0xb4},
	{0x93,0x00},
	{0x94,0xf0},
	{0x95,0x00},
	{0x96,0xf0},
	{0x97,0x01},
	{0x98,0x40},
	// AWB
	{0xfe,0x00},
	{0xec,0x01},
	{0xed,0x02},
	{0xee,0x30},
	{0xef,0x48},
	{0xfe,0x01},
	{0x74,0x00},
	//// AEC
	{0xfe,0x01},
	{0x01,0x04},
	{0x02,0x60},
	{0x03,0x02},
	{0x04,0x48},
	{0x05,0x18},
	{0x06,0x4c},
	{0x07,0x14},
	{0x08,0x36},
	{0x0a,0xc0},
	{0x21,0x14},
#if 0
	{0x25,0x01},
	{0x26,0x90},
	{0x27,0x03},
	{0x28,0x20}, //50fps
	{0x29,0x03},
	{0x2a,0x20},
	{0x2b,0x03},
	{0x2c,0x20},
	{0x2d,0x03},
	{0x2e,0x20},
#endif
	{0xfe,0x00},
	//// gamma
	{0xfe,0x00},
	{0xc3,0x11},
	{0xc4,0x20},
	{0xc5,0x30},
	{0xfe,0x00},
	ENDMARKER,
};

/* vga center crop */
static const struct regval_list gc2155_vga_regs[] = {
	{0xfe,0x00},
	{0xfd,0x01},
	// crop window
	{0xfe,0x00},
	{0x90,0x01},
	{0x91,0x00},
	{0x92,0x3c},
	{0x93,0x00},
	{0x94,0x50},
	{0x95,0x01},
	{0x96,0xe0},
	{0x97,0x02},
	{0x98,0x80},
	// AWB
	{0xfe,0x00},
	{0xec,0x01},
	{0xed,0x02},
	{0xee,0x30},
	{0xef,0x48},
	{0xfe,0x01},
	{0x74,0x00},
	//// AEC
	{0xfe,0x01},
	{0x01,0x04},
	{0x02,0x60},
	{0x03,0x02},
	{0x04,0x48},
	{0x05,0x18},
	{0x06,0x4c},
	{0x07,0x14},
	{0x08,0x36},
	{0x0a,0xc0},
	{0x21,0x14},
#if 0
	{0x25,0x01},
	{0x26,0x90},
	{0x27,0x03},
	{0x28,0x20}, //50fps
	{0x29,0x03},
	{0x2a,0x20},
	{0x2b,0x03},
	{0x2c,0x20},
	{0x2d,0x03},
	{0x2e,0x20},
#endif
	{0xfe,0x00},
	//// gamma
	{0xfe,0x00},
	{0xc3,0x11},
	{0xc4,0x20},
	{0xc5,0x30},
	{0xfe,0x00},
	ENDMARKER,
};

static const struct regval_list gc2155_720p_regs[] = {
	// 720P init
	 {0xfe,0x00},
	 {0xb6,0x01},
	 {0xfd,0x00},

  //subsample
	 {0xfe,0x00},
	 {0x99,0x55},
	 {0x9a,0x06},
	 {0x9b,0x00},
	 {0x9c,0x00},
	 {0x9d,0x01},
	 {0x9e,0x23},
	 {0x9f,0x00},
	 {0xa0,0x00},
	 {0xa1,0x01},
	 {0xa2,0x23},
	 //crop window
	 {0x90,0x01},
	 {0x91,0x00},
	 {0x92,0x78},
	 {0x93,0x00},
	 {0x94,0x00},
	 {0x95,0x02},
	 {0x96,0xd0},
	 {0x97,0x05},
	 {0x98,0x00},
	 // AWB
	 {0xfe,0x00},
	 {0xec,0x02},
	 {0xed,0x04},
	 {0xee,0x60},
	 {0xef,0x90},
	 {0xfe,0x01},
	 {0x74,0x01},
	 // AEC
	 {0xfe,0x01},
	 {0x01,0x08},
	 {0x02,0xc0},
	 {0x03,0x04},
	 {0x04,0x90},
	 {0x05,0x30},
	 {0x06,0x98},
	 {0x07,0x28},
	 {0x08,0x6c},
	 {0x0a,0xc2},
	 {0x21,0x15},
	 {0xfe,0x00},
	 //banding setting 20fps fixed///
	 {0xfe,0x00},
	 {0x03,0x03},
	 {0x04,0xe8},
	 {0x05,0x01},
	 {0x06,0x56},
	 {0x07,0x00},
	 {0x08,0x32},
	 {0xfe,0x01},
	 {0x25,0x00},
	 {0x26,0xfa},
	 {0x27,0x04},
	 {0x28,0xe2}, //20fps
	 {0x29,0x04},
	 {0x2a,0xe2}, //16fps   5dc
	 {0x2b,0x04},
	 {0x2c,0xe2}, //16fps  6d6  5dc
	 {0x2d,0x04},
	 {0x2e,0xe2}, //8fps	bb8
	 {0x3c,0x00}, //8fps
	 {0xfe,0x00},
	ENDMARKER,
};

static const struct regval_list gc2155_wb_auto_regs[] = {
	ENDMARKER,
};

static const struct regval_list gc2155_wb_incandescence_regs[] = {
	ENDMARKER,
};

static const struct regval_list gc2155_wb_daylight_regs[] = {
	ENDMARKER,
};

static const struct regval_list gc2155_wb_fluorescent_regs[] = {
	ENDMARKER,
};

static const struct regval_list gc2155_wb_cloud_regs[] = {
	ENDMARKER,
};

static const struct mode_list gc2155_balance[] = {
	{0, gc2155_wb_auto_regs}, {1, gc2155_wb_incandescence_regs},
	{2, gc2155_wb_daylight_regs}, {3, gc2155_wb_fluorescent_regs},
	{4, gc2155_wb_cloud_regs},
};


static const struct regval_list gc2155_effect_normal_regs[] = {
	ENDMARKER,
};

static const struct regval_list gc2155_effect_grayscale_regs[] = {
	ENDMARKER,
};

static const struct regval_list gc2155_effect_sepia_regs[] = {
	ENDMARKER,
};

static const struct regval_list gc2155_effect_colorinv_regs[] = {
	ENDMARKER,
};

static const struct regval_list gc2155_effect_sepiabluel_regs[] = {
	ENDMARKER,
};

static const struct mode_list gc2155_effect[] = {
	{0, gc2155_effect_normal_regs}, {1, gc2155_effect_grayscale_regs},
	{2, gc2155_effect_sepia_regs}, {3, gc2155_effect_colorinv_regs},
	{4, gc2155_effect_sepiabluel_regs},
};

#define GC2155_SIZE(n, w, h, r) \
	{.name = n, .width = w , .height = h, .regs = r }

static const struct gc2155_win_size gc2155_supported_win_sizes[] = {
	GC2155_SIZE("QVGA", W_QVGA, H_QVGA, gc2155_qvga_regs),
	GC2155_SIZE("VGA", W_VGA, H_VGA, gc2155_vga_regs),
	GC2155_SIZE("720P", W_720P, H_720P, gc2155_720p_regs),
};

#define N_WIN_SIZES (ARRAY_SIZE(gc2155_supported_win_sizes))

static const struct regval_list gc2155_yuv422_regs[] = {
	ENDMARKER,
};

static const struct regval_list gc2155_rgb565_regs[] = {
	ENDMARKER,
};

static u32 gc2155_codes[] = {
	MEDIA_BUS_FMT_YUYV8_2X8,
	MEDIA_BUS_FMT_YUYV8_1_5X8,
	MEDIA_BUS_FMT_UYVY8_1X16,
};

/*
 * Supported balance menus
 */
static const struct v4l2_querymenu gc2155_balance_menus[] = {
	{
		.id		= V4L2_CID_PRIVATE_BALANCE,
		.index		= 0,
		.name		= "auto",
	}, {
		.id		= V4L2_CID_PRIVATE_BALANCE,
		.index		= 1,
		.name		= "incandescent",
	}, {
		.id		= V4L2_CID_PRIVATE_BALANCE,
		.index		= 2,
		.name		= "fluorescent",
	},  {
		.id		= V4L2_CID_PRIVATE_BALANCE,
		.index		= 3,
		.name		= "daylight",
	},  {
		.id		= V4L2_CID_PRIVATE_BALANCE,
		.index		= 4,
		.name		= "cloudy-daylight",
	},

};

/*
 * Supported effect menus
 */
static const struct v4l2_querymenu gc2155_effect_menus[] = {
	{
		.id		= V4L2_CID_PRIVATE_EFFECT,
		.index		= 0,
		.name		= "none",
	}, {
		.id		= V4L2_CID_PRIVATE_EFFECT,
		.index		= 1,
		.name		= "mono",
	}, {
		.id		= V4L2_CID_PRIVATE_EFFECT,
		.index		= 2,
		.name		= "sepia",
	},  {
		.id		= V4L2_CID_PRIVATE_EFFECT,
		.index		= 3,
		.name		= "negative",
	}, {
		.id		= V4L2_CID_PRIVATE_EFFECT,
		.index		= 4,
		.name		= "aqua",
	},
};


/*
 * General functions
 */
static struct gc2155_priv *to_gc2155(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct gc2155_priv,
			    subdev);
}

static int gc2155_write_array(struct i2c_client *client,
			      const struct regval_list *vals)
{
	int ret;

	while ((vals->reg_num != 0xff) || (vals->value != 0xff)) {
		ret = gc2155_write_reg(client, vals->reg_num, vals->value);
		dev_vdbg(&client->dev, "array: 0x%02x, 0x%02x",
			 vals->reg_num, vals->value);

//		msleep(100);
//		my_num = gc2155_read_reg(client, vals->reg_num);
//		if (my_num != vals->value)
//		{
//			printk("vals_old->reg_num is 0x%x, vals_old->value is 0x%x, ret is 0x%x\n", vals->reg_num, vals->value, my_num);
//		}
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int gc2155_mask_set(struct i2c_client *client,
			   u8  reg, u8  mask, u8  set)
{
	s32 val = gc2155_read_reg(client, reg);
	if (val < 0)
		return val;

	val &= ~mask;
	val |= set & mask;

	dev_vdbg(&client->dev, "masks: 0x%02x, 0x%02x", reg, val);

	return gc2155_write_reg(client, reg, val);
}

static int gc2155_reset(struct i2c_client *client)
{
	return 0;
}

/*
 * soc_camera_ops functions
 */
static int gc2155_s_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static int gc2155_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd =
		&container_of(ctrl->handler, struct gc2155_priv, hdl)->subdev;
	struct i2c_client  *client = v4l2_get_subdevdata(sd);
	struct gc2155_priv *priv = to_gc2155(client);

	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
		ctrl->val = priv->flag_vflip;
		break;
	case V4L2_CID_HFLIP:
		ctrl->val = priv->flag_hflip;
		break;
	case V4L2_CID_PRIVATE_BALANCE:
		ctrl->val = priv->balance_value;
		break;
	case V4L2_CID_PRIVATE_EFFECT:
		ctrl->val = priv->effect_value;
		break;
	default:
		break;
	}
	return 0;
}

static int gc2155_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd =
		&container_of(ctrl->handler, struct gc2155_priv, hdl)->subdev;
	struct i2c_client  *client = v4l2_get_subdevdata(sd);
	struct gc2155_priv *priv = to_gc2155(client);
	int ret = 0;
	int i = 0;
	u8 value;

	int balance_count = ARRAY_SIZE(gc2155_balance);
	int effect_count = ARRAY_SIZE(gc2155_effect);

	switch (ctrl->id) {
	case V4L2_CID_PRIVATE_BALANCE:
		if(ctrl->val > balance_count)
			return -EINVAL;

		for(i = 0; i < balance_count; i++) {
			if(ctrl->val == gc2155_balance[i].index) {
				ret = gc2155_write_array(client,
						gc2155_balance[ctrl->val].mode_regs);
				priv->balance_value = ctrl->val;
				break;
			}
		}
		break;

	case V4L2_CID_PRIVATE_EFFECT:
		if(ctrl->val > effect_count)
			return -EINVAL;

		for(i = 0; i < effect_count; i++) {
			if(ctrl->val == gc2155_effect[i].index) {
				ret = gc2155_write_array(client,
						gc2155_effect[ctrl->val].mode_regs);
				priv->effect_value = ctrl->val;
				break;
			}
		}
		break;

	case V4L2_CID_VFLIP:
		value = ctrl->val ? REG14_VFLIP_IMG : 0x00;
		priv->flag_vflip = ctrl->val ? 1 : 0;
		ret = gc2155_mask_set(client, REG14, REG14_VFLIP_IMG, value);
		break;

	case V4L2_CID_HFLIP:
		value = ctrl->val ? REG14_HFLIP_IMG : 0x00;
		priv->flag_hflip = ctrl->val ? 1 : 0;
		ret = gc2155_mask_set(client, REG14, REG14_HFLIP_IMG, value);
		break;

	default:
		dev_err(&client->dev, "no V4L2 CID: 0x%x ", ctrl->id);
		return -EINVAL;
	}

	return ret;
}

static int gc2155_querymenu(struct v4l2_subdev *sd,
					struct v4l2_querymenu *qm)
{
	switch (qm->id) {
	case V4L2_CID_PRIVATE_BALANCE:
		memcpy(qm->name, gc2155_balance_menus[qm->index].name,
				sizeof(qm->name));
		break;

	case V4L2_CID_PRIVATE_EFFECT:
		memcpy(qm->name, gc2155_effect_menus[qm->index].name,
				sizeof(qm->name));
		break;
	}

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int gc2155_g_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	reg->size = 1;
	if (reg->reg > 0xff)
		return -EINVAL;

	ret = gc2155_read_reg(client, reg->reg);
	if (ret < 0)
		return ret;

	reg->val = ret;

	return 0;
}

static int gc2155_s_register(struct v4l2_subdev *sd,
			     const struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (reg->reg > 0xff ||
	    reg->val > 0xff)
		return -EINVAL;

	return gc2155_write_reg(client, reg->reg, reg->val);
}
#endif

/* Select the nearest higher resolution for capture */
static const struct gc2155_win_size *gc2155_select_win(u32 *width, u32 *height)
{
	int i, default_size = ARRAY_SIZE(gc2155_supported_win_sizes) - 1;

	for (i = 0; i < ARRAY_SIZE(gc2155_supported_win_sizes); i++) {
		if ((gc2155_supported_win_sizes[i].width >= *width) &&
		    (gc2155_supported_win_sizes[i].height >= *height)) {
			*width = gc2155_supported_win_sizes[i].width;
			*height = gc2155_supported_win_sizes[i].height;
			return &gc2155_supported_win_sizes[i];
		}
	}

	*width = gc2155_supported_win_sizes[default_size].width;
	*height = gc2155_supported_win_sizes[default_size].height;
	return &gc2155_supported_win_sizes[default_size];
}

static int gc2155_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client  *client = v4l2_get_subdevdata(sd);
	struct gc2155_priv *priv = to_gc2155(client);

	if (format->pad)
		return -EINVAL;

	if (!priv->win) {
		u32 width = W_VGA, height = H_VGA;
		priv->win = gc2155_select_win(&width, &height);
		priv->cfmt_code = MEDIA_BUS_FMT_YUYV8_2X8;
	}

	mf->width	= priv->win->width;
	mf->height	= priv->win->height;
	mf->code	= priv->cfmt_code;

	mf->colorspace = V4L2_COLORSPACE_JPEG;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int gc2155_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (format->pad)
		return -EINVAL;

	/*
	 * select suitable win, but don't store it
	 */
	gc2155_select_win(&mf->width, &mf->height);

	mf->field	= V4L2_FIELD_NONE;

	switch (mf->code) {
	case MEDIA_BUS_FMT_RGB565_2X8_BE:
	case MEDIA_BUS_FMT_RGB565_2X8_LE:
		mf->colorspace = V4L2_COLORSPACE_SRGB;
		break;
	default:
		mf->code = MEDIA_BUS_FMT_UYVY8_2X8;
	case MEDIA_BUS_FMT_YUYV8_2X8:
	case MEDIA_BUS_FMT_UYVY8_2X8:
		mf->colorspace = V4L2_COLORSPACE_JPEG;
	}

	if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		return gc2155_set_params(client, &mf->width,
					 &mf->height, mf->code);
	cfg->try_fmt = *mf;
	return 0;
}

static int gc2155_set_params(struct i2c_client *client, u32 *width, u32 *height, u32 code)
{
	struct gc2155_priv       *priv = to_gc2155(client);
	int ret;

	int bala_index = priv->balance_value;
	int effe_index = priv->effect_value;

	/* select win */
	priv->win = gc2155_select_win(width, height);

	/* select format */
	priv->cfmt_code = 0;

	/* reset hardware */
	gc2155_reset(client);

	/* initialize the sensor with default data */
	dev_dbg(&client->dev, "%s: Init default", __func__);
	ret = gc2155_write_array(client, gc2155_init_regs);
	if (ret < 0)
		goto err;

	/* set balance */
	ret = gc2155_write_array(client, gc2155_balance[bala_index].mode_regs);
	if (ret < 0)
		goto err;

	/* set effect */
	ret = gc2155_write_array(client, gc2155_effect[effe_index].mode_regs);
	if (ret < 0)
		goto err;

	/* set size win */
	ret = gc2155_write_array(client, priv->win->regs);
	if (ret < 0)
		goto err;

	priv->cfmt_code = code;
	*width = priv->win->width;
	*height = priv->win->height;

	return 0;

err:
	dev_err(&client->dev, "%s: Error %d", __func__, ret);
	gc2155_reset(client);
	priv->win = NULL;

	return ret;
}

static int gc2155_enum_mbus_code(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index >= ARRAY_SIZE(gc2155_codes))
		return -EINVAL;

	code->code = gc2155_codes[code->index];
	return 0;
}

static int gc2155_enum_frame_size(struct v4l2_subdev *sd,
		       struct v4l2_subdev_pad_config *cfg,
		       struct v4l2_subdev_frame_size_enum *fse)
{
	int i, j;
	int num_valid = -1;
	__u32 index = fse->index;

	if(index >= N_WIN_SIZES)
		return -EINVAL;

	j = ARRAY_SIZE(gc2155_codes);
	while(--j)
		if(fse->code == gc2155_codes[j])
			break;

	for (i = 0; i < N_WIN_SIZES; i++) {
		if (index == ++num_valid) {
			fse->code = gc2155_codes[j];
			fse->min_width = gc2155_supported_win_sizes[index].width;
			fse->max_width = fse->min_width;
			fse->min_height = gc2155_supported_win_sizes[index].height;
			fse->max_height = fse->min_height;
			return 0;
		}
	}

	return -EINVAL;
}


static int gc2155_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= W_720P;
	a->c.height	= H_720P;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int gc2155_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= W_720P;
	a->bounds.height		= H_720P;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int gc2155_video_probe(struct i2c_client *client)
{
	unsigned char retval_high = 0, retval_low = 0;
	struct gc2155_priv *priv = to_gc2155(client);
	int ret;

	ret = gc2155_s_power(&priv->subdev, 1);
	if (ret < 0)
		return ret;
	/*
	 * check and show product ID and manufacturer ID
	 */

	retval_high = gc2155_read_reg(client, REG_CHIP_ID_HIGH);
	if (retval_high != CHIP_ID_HIGH) {
		dev_err(&client->dev, "read sensor %s chip_id high %x is error\n",
				client->name, retval_high);
		ret = -EINVAL;
		goto done;
	}


	retval_low = gc2155_read_reg(client, REG_CHIP_ID_LOW);
	if (retval_low != CHIP_ID_LOW) {
		dev_err(&client->dev, "read sensor %s chip_id low %x is error\n",
				client->name, retval_low);
		ret = -EINVAL;
		goto done;
	}

	dev_info(&client->dev, "read sensor %s id high:0x%x,low:%x successed!\n",
			client->name, retval_high, retval_low);

	ret = v4l2_ctrl_handler_setup(&priv->hdl);

done:
	gc2155_s_power(&priv->subdev, 0);
	return ret;
}

static int gc2155_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);
	struct gc2155_priv *priv = to_gc2155(client);

	return soc_camera_set_power(&client->dev, ssdd, priv->clk, on);
}

static const struct v4l2_ctrl_ops gc2155_ctrl_ops = {
	.s_ctrl = gc2155_s_ctrl,
	.g_volatile_ctrl = gc2155_g_ctrl,
};

static struct v4l2_subdev_core_ops gc2155_subdev_core_ops = {
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= gc2155_g_register,
	.s_register	= gc2155_s_register,
#endif
	.s_power	= gc2155_s_power,
	.querymenu	= gc2155_querymenu,
};

static int gc2155_g_mbus_config(struct v4l2_subdev *sd,
		struct v4l2_mbus_config *cfg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);

	cfg->flags = V4L2_MBUS_PCLK_SAMPLE_FALLING | V4L2_MBUS_MASTER |
		    V4L2_MBUS_VSYNC_ACTIVE_LOW | V4L2_MBUS_HSYNC_ACTIVE_HIGH |
			    V4L2_MBUS_DATA_ACTIVE_HIGH;
	cfg->type = V4L2_MBUS_PARALLEL;

	cfg->flags = soc_camera_apply_board_flags(ssdd, cfg);

	return 0;
}

static struct v4l2_subdev_video_ops gc2155_subdev_video_ops = {
	.s_stream	= gc2155_s_stream,
	.cropcap	= gc2155_cropcap,
	.g_crop		= gc2155_g_crop,
	.g_mbus_config	= gc2155_g_mbus_config,
};

static const struct v4l2_subdev_pad_ops gc2155_subdev_pad_ops = {
	.enum_mbus_code = gc2155_enum_mbus_code,
	.enum_frame_size = gc2155_enum_frame_size,
	.get_fmt	= gc2155_get_fmt,
	.set_fmt	= gc2155_set_fmt,
};

static struct v4l2_subdev_ops gc2155_subdev_ops = {
	.core	= &gc2155_subdev_core_ops,
	.video	= &gc2155_subdev_video_ops,
	.pad	= &gc2155_subdev_pad_ops,
};

/* OF probe functions */
static int gc2155_hw_power(struct device *dev, int on)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gc2155_priv *priv = to_gc2155(client);

	dev_dbg(&client->dev, "%s: %s the camera\n",
			__func__, on ? "ENABLE" : "DISABLE");

	/* thses gpio should be set according to the active level in dt defines */
	if(priv->vcc_en_gpio) {
		gpiod_direction_output(priv->vcc_en_gpio, on);
	}

	if (priv->pwdn_gpio) {
		gpiod_direction_output(priv->pwdn_gpio, !on);
	}

	msleep(10);
	return 0;
}

static int gc2155_hw_reset(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gc2155_priv *priv = to_gc2155(client);

	if (priv->resetb_gpio) {
		/* Active the resetb pin to perform a reset pulse */
		gpiod_direction_output(priv->resetb_gpio, 1);
		usleep_range(3000, 5000);
		gpiod_direction_output(priv->resetb_gpio, 0);
	}

	return 0;
}


static int gc2155_probe_dt(struct i2c_client *client,
		struct gc2155_priv *priv)
{

	struct soc_camera_subdev_desc	*ssdd_dt = &priv->ssdd_dt;
	struct v4l2_subdev_platform_data *sd_pdata = &ssdd_dt->sd_pdata;
	struct device_node *np = client->dev.of_node;
	int supplies = 0, index = 0;

	supplies = of_property_count_strings(np, "supplies-name");
	if(supplies <= 0) {
		goto no_supply;
	}

	sd_pdata->num_regulators = supplies;
	sd_pdata->regulators = devm_kzalloc(&client->dev, supplies * sizeof(struct regulator_bulk_data), GFP_KERNEL);
	if(!sd_pdata->regulators) {
		dev_err(&client->dev, "Failed to allocate regulators.!\n");
		goto no_supply;
	}

	for(index = 0; index < sd_pdata->num_regulators; index ++) {
		of_property_read_string_index(np, "supplies-name", index,
				&(sd_pdata->regulators[index].supply));

		dev_dbg(&client->dev, "sd_pdata->regulators[%d].supply: %s\n",
				index, sd_pdata->regulators[index].supply);
	}

	soc_camera_power_init(&client->dev, ssdd_dt);

no_supply:

	/* Request the reset GPIO deasserted */
	priv->resetb_gpio = devm_gpiod_get_optional(&client->dev, "resetb",
			GPIOD_OUT_LOW);
	if (!priv->resetb_gpio)
		dev_dbg(&client->dev, "resetb gpio is not assigned!\n");
	else if (IS_ERR(priv->resetb_gpio))
		return PTR_ERR(priv->resetb_gpio);

	/* Request the power down GPIO asserted */
	priv->pwdn_gpio = devm_gpiod_get_optional(&client->dev, "pwdn",
			GPIOD_OUT_HIGH);
	if (!priv->pwdn_gpio)
		dev_dbg(&client->dev, "pwdn gpio is not assigned!\n");
	else if (IS_ERR(priv->pwdn_gpio))
		return PTR_ERR(priv->pwdn_gpio);

	/* Request the power down GPIO asserted */
	priv->vcc_en_gpio = devm_gpiod_get_optional(&client->dev, "vcc-en",
			GPIOD_OUT_HIGH);
	if (!priv->vcc_en_gpio)
		dev_dbg(&client->dev, "vcc_en gpio is not assigned!\n");
	else if (IS_ERR(priv->vcc_en_gpio))
		return PTR_ERR(priv->vcc_en_gpio);

	/* Initialize the soc_camera_subdev_desc */
	priv->ssdd_dt.power = gc2155_hw_power;
	priv->ssdd_dt.reset = gc2155_hw_reset;
	client->dev.platform_data = &priv->ssdd_dt;
	return 0;
}

#include <linux/regulator/consumer.h>
/*
 * i2c_driver functions
 */
static int gc2155_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct gc2155_priv	*priv;
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);
	struct i2c_adapter	*adapter = to_i2c_adapter(client->dev.parent);
	int	ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&adapter->dev,
			"GC2155: I2C-Adapter doesn't support SMBUS\n");
		return -EIO;
	}

	priv = devm_kzalloc(&client->dev, sizeof(struct gc2155_priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&adapter->dev,
			"Failed to allocate memory for private data!\n");
		return -ENOMEM;
	}

	priv->clk = v4l2_clk_get(&client->dev, "cgu_cim");
	if (IS_ERR(priv->clk))
		return -EPROBE_DEFER;

	v4l2_clk_set_rate(priv->clk, 24000000);

	if (!ssdd && !client->dev.of_node) {
		dev_err(&client->dev, "Missing platform_data for driver\n");
		ret = -EINVAL;
		goto err_videoprobe;
	}

	if (!ssdd) {
		ret = gc2155_probe_dt(client, priv);
		if (ret)
			goto err_clk;
	}


	v4l2_i2c_subdev_init(&priv->subdev, client, &gc2155_subdev_ops);

	/* add handler */
	v4l2_ctrl_handler_init(&priv->hdl, 2);
	v4l2_ctrl_new_std(&priv->hdl, &gc2155_ctrl_ops,
			V4L2_CID_VFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&priv->hdl, &gc2155_ctrl_ops,
			V4L2_CID_HFLIP, 0, 1, 1, 0);
	priv->subdev.ctrl_handler = &priv->hdl;
	if (priv->hdl.error) {
		ret = priv->hdl.error;
		goto err_clk;
	}

	ret = gc2155_video_probe(client);
	if (ret < 0)
		goto err_videoprobe;

	ret = v4l2_async_register_subdev(&priv->subdev);
	if (ret < 0)
		goto err_videoprobe;

	dev_info(&adapter->dev, "GC2155 Probed\n");

	return 0;

err_videoprobe:
	v4l2_ctrl_handler_free(&priv->hdl);
err_clk:
	v4l2_clk_put(priv->clk);
	return ret;
}

static int gc2155_remove(struct i2c_client *client)
{
	struct gc2155_priv       *priv = to_gc2155(client);

	v4l2_async_unregister_subdev(&priv->subdev);
	v4l2_clk_put(priv->clk);
	v4l2_device_unregister_subdev(&priv->subdev);
	v4l2_ctrl_handler_free(&priv->hdl);
	return 0;
}

static const struct i2c_device_id gc2155_id[] = {
	{ "gc2155",  0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc2155_id);
static const struct of_device_id gc2155_of_match[] = {
	{.compatible = "galaxycore,gc2155", },
	{},
};
MODULE_DEVICE_TABLE(of, gc2155_of_match);
static struct i2c_driver gc2155_i2c_driver = {
	.driver = {
		.name = "gc2155",
		.of_match_table = of_match_ptr(gc2155_of_match),
	},
	.probe    = gc2155_probe,
	.remove   = gc2155_remove,
	.id_table = gc2155_id,
};
module_i2c_driver(gc2155_i2c_driver);

MODULE_DESCRIPTION("SoC Camera driver for galaxycore gc2155 sensor");
MODULE_AUTHOR("Alberto Panizzo");
MODULE_LICENSE("GPL v2");
