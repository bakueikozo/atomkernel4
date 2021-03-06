/*
 * File: drivers/video/ingenic/uapi_ingenicfb.h
 *
 *
 * Copyright (C) 2017 Ingenic Semiconductor Inc.
 *
 * Author:clwang<chunlei.wang@ingenic.com>
 * 	  qipengzhen<aric.pzqi@ingenic.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#ifndef __UAPI_LINUX_INGENICFB_H__
#define __UAPI_LINUX_INGENICFB_H__

#define MAX_LAYER_NUM 2
#define PIXEL_ALIGN 4

enum {
	LAYER_CFG_FORMAT_RGB555 = 0,
	LAYER_CFG_FORMAT_ARGB1555 = 1,
	LAYER_CFG_FORMAT_RGB565	= 2,
	LAYER_CFG_FORMAT_RGB888	= 4,
	LAYER_CFG_FORMAT_ARGB8888 = 5,
	LAYER_CFG_FORMAT_NV12 = 8,
	LAYER_CFG_FORMAT_NV21 = 9,
	LAYER_CFG_FORMAT_YUV422 = 10,
	LAYER_CFG_FORMAT_TILE_H264 = 12,
};

enum {
	LAYER_CFG_COLOR_RGB = 0,
	LAYER_CFG_COLOR_RBG = 1,
	LAYER_CFG_COLOR_GRB = 2,
	LAYER_CFG_COLOR_GBR = 3,
	LAYER_CFG_COLOR_BRG = 4,
	LAYER_CFG_COLOR_BGR = 5,
};
/**
* @ingenicfb_layer_cfg layer cfg for frames. userspace use this struct.
*
* @layer_en layer enable.
* @layer_z_order
* @pic_width width of this layer
* @pic_height height of this layer
* @disp_pos_x	start x
* @disp_pos_y	start y.
* @g_alpha_en	global alpha enable.
* @g_alpha_val	global alpha value.
* @color	color of source data in memory. if format is not RGB kind.
* 		color is useless.
* @format	format of source data in memory.
* @stride	width stride.
*/
struct ingenicfb_lay_cfg {
	unsigned int lay_en;
	unsigned int lay_z_order;
	unsigned int pic_width;
	unsigned int pic_height;
	unsigned int disp_pos_x;
	unsigned int disp_pos_y;
	unsigned int g_alpha_en;
	unsigned int g_alpha_val;
	unsigned int color;
	unsigned int domain_multi;
	unsigned int format;
	unsigned int stride;
};
struct ingenicfb_frm_cfg {
	struct ingenicfb_lay_cfg lay_cfg[MAX_LAYER_NUM];
};

typedef enum csc_mode {
	CSC_MODE_0,
	CSC_MODE_1,
	CSC_MODE_2,
	CSC_MODE_3,
} csc_mode_t;

struct ingenicfb_timestamp {
#define TIMESTAMP_CAP	(16)
	int wp;	/* Write position, after lcd write a frame to lcd. update wp*/
	int rp; /* Read position, after wait vsync, update rp*/
	unsigned long long value[TIMESTAMP_CAP];
};

#define JZFB_PUT_FRM_CFG		_IOWR('F', 0x101, struct ingenicfb_frm_cfg *)
#define JZFB_GET_FRM_CFG		_IOWR('F', 0x102, struct ingenicfb_frm_cfg *)
#define JZFB_SET_LAYER_POS		_IOWR('F', 0x105, struct ingenicfb_layer_pos *)
#define JZFB_SET_LAYER_SIZE		_IOWR('F', 0x106, struct ingenicfb_layer_size *)
#define JZFB_SET_LAYER_FORMAT		_IOWR('F', 0x109, struct ingenicfb_layer_format *)
#define JZFB_SET_LAYER_ALPHA		_IOWR('F', 0x151, struct ingenicfb_layer_alpha *)

#define JZFB_GET_LAYER_POS		_IOR('F', 0x115, struct ingenicfb_layer_pos *)
#define JZFB_GET_LAYER_SIZE		_IOR('F', 0x116, struct ingenicfb_layer_size *)

#define JZFB_SET_CSC_MODE		_IOW('F', 0x120, csc_mode_t)
#define JZFB_CMP_START			_IOW('F', 0X141, int)
#define JZFB_TFT_START			_IOW('F', 0X143, int)
#define JZFB_SLCD_START			_IOW('F', 0X144, int)
#define JZFB_GEN_STP_CMP		_IOW('F', 0x145, int)
#define JZFB_QCK_STP_CMP		_IOW('F', 0x147, int)
#define JZFB_DUMP_LCDC_REG		_IOW('F', 0x150, int)

#define JZFB_SET_VSYNCINT		_IOW('F', 0x210, int)

#endif
