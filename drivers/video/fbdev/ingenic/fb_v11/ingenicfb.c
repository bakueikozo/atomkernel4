/*
 * File: drivers/video/ingenic/x2000/ingenicfb.c
 *
 *
 * Copyright (C) 2016 Ingenic Semiconductor Inc.
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


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/suspend.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/of_address.h>

#include "dpu_reg.h"
#include "ingenicfb.h"

#define CONFIG_FB_JZ_DEBUG

struct lcd_panel *fbdev_panel = NULL;
struct platform_device *fbdev_pdev = NULL;

static int uboot_inited;
static int showFPS = 0;
static struct ingenicfb_device *fbdev;

static const struct fb_fix_screeninfo ingenicfb_fix  = {
	.id = "ingenicfb",
	.type = FB_TYPE_PACKED_PIXELS,
	.visual = FB_VISUAL_TRUECOLOR,
	.xpanstep = 0,
	.ypanstep = 1,
	.ywrapstep = 0,
	.accel = FB_ACCEL_NONE,
};

struct ingenicfb_colormode {
	uint32_t mode;
	uint32_t color;
	uint32_t bits_per_pixel;
	uint32_t nonstd;
	struct fb_bitfield red;
	struct fb_bitfield green;
	struct fb_bitfield blue;
	struct fb_bitfield transp;
};

static struct ingenicfb_colormode ingenicfb_colormodes[] = {
	{
		.mode = LAYER_CFG_FORMAT_RGB888,
		.color = LAYER_CFG_COLOR_RGB,
		.bits_per_pixel = 32,
		.nonstd = 0,
#ifdef CONFIG_ANDROID
		.color = LAYER_CFG_COLOR_BGR,
		.red	= { .length = 8, .offset = 0, .msb_right = 0 },
		.green	= { .length = 8, .offset = 8, .msb_right = 0 },
		.blue	= { .length = 8, .offset = 16, .msb_right = 0 },
#else
		.color = LAYER_CFG_COLOR_RGB,
		.red	= { .length = 8, .offset = 16, .msb_right = 0 },
		.green	= { .length = 8, .offset = 8, .msb_right = 0 },
		.blue	= { .length = 8, .offset = 0, .msb_right = 0 },
#endif
		.transp	= { .length = 0, .offset = 0, .msb_right = 0 },
	}, {
		.mode = LAYER_CFG_FORMAT_ARGB8888,
		.bits_per_pixel = 32,
		.nonstd = 0,
#ifdef CONFIG_ANDROID
		.color = LAYER_CFG_COLOR_BGR,
		.red	= { .length = 8, .offset = 0, .msb_right = 0 },
		.green	= { .length = 8, .offset = 8, .msb_right = 0 },
		.blue	= { .length = 8, .offset = 16, .msb_right = 0 },
#else
		.color = LAYER_CFG_COLOR_RGB,
		.red	= { .length = 8, .offset = 16, .msb_right = 0 },
		.green	= { .length = 8, .offset = 8, .msb_right = 0 },
		.blue	= { .length = 8, .offset = 0, .msb_right = 0 },
#endif
		.transp	= { .length = 8, .offset = 24, .msb_right = 0 },
	}, {
		.mode = LAYER_CFG_FORMAT_RGB555,
		.color = LAYER_CFG_COLOR_RGB,
		.bits_per_pixel = 16,
		.nonstd = 0,
		.red	= { .length = 5, .offset = 10, .msb_right = 0 },
		.green	= { .length = 5, .offset = 5, .msb_right = 0 },
		.blue	= { .length = 5, .offset = 0, .msb_right = 0 },
		.transp	= { .length = 0, .offset = 0, .msb_right = 0 },
	}, {
		.mode = LAYER_CFG_FORMAT_ARGB1555,
		.color = LAYER_CFG_COLOR_RGB,
		.bits_per_pixel = 16,
		.nonstd = 0,
		.red	= { .length = 5, .offset = 10, .msb_right = 0 },
		.green	= { .length = 5, .offset = 5, .msb_right = 0 },
		.blue	= { .length = 5, .offset = 0, .msb_right = 0 },
		.transp	= { .length = 1, .offset = 15, .msb_right = 0 },
	}, {
		.mode = LAYER_CFG_FORMAT_RGB565,
		.color = LAYER_CFG_COLOR_RGB,
		.bits_per_pixel = 16,
		.nonstd = 0,
		.red	= { .length = 5, .offset = 11, .msb_right = 0 },
		.green	= { .length = 6, .offset = 5, .msb_right = 0 },
		.blue	= { .length = 5, .offset = 0, .msb_right = 0 },
		.transp	= { .length = 0, .offset = 0, .msb_right = 0 },
	}, {
		.mode = LAYER_CFG_FORMAT_YUV422,
		.bits_per_pixel = 16,
		.nonstd = LAYER_CFG_FORMAT_YUV422,
	},
};

static void dump_dc_reg(void)
{
	printk("-----------------dc_reg------------------\n");
	printk("DC_FRM_CFG_ADDR:    %lx\n",reg_read(fbdev, DC_FRM_CFG_ADDR));
	printk("DC_FRM_CFG_CTRL:    %lx\n",reg_read(fbdev, DC_FRM_CFG_CTRL));
	printk("DC_CTRL:            %lx\n",reg_read(fbdev, DC_CTRL));
	printk("DC_CSC_MULT_YRV:    %lx\n",reg_read(fbdev, DC_CSC_MULT_YRV));
	printk("DC_CSC_MULT_GUGV:   %lx\n",reg_read(fbdev, DC_CSC_MULT_GUGV));
	printk("DC_CSC_MULT_BU:     %lx\n",reg_read(fbdev, DC_CSC_MULT_BU));
	printk("DC_CSC_SUB_YUV:     %lx\n",reg_read(fbdev, DC_CSC_SUB_YUV));
	printk("DC_ST:              %lx\n",reg_read(fbdev, DC_ST));
	printk("DC_INTC:            %lx\n",reg_read(fbdev, DC_INTC));
	printk("DC_INT_FLAG:	    %lx\n",reg_read(fbdev, DC_INT_FLAG));
	printk("DC_COM_CONFIG:      %lx\n",reg_read(fbdev, DC_COM_CONFIG));
	printk("DC_PCFG_RD_CTRL:    %lx\n",reg_read(fbdev, DC_PCFG_RD_CTRL));
	printk("DC_OFIFO_PCFG:	    %lx\n",reg_read(fbdev, DC_OFIFO_PCFG));
	printk("DC_DISP_COM:        %lx\n",reg_read(fbdev, DC_DISP_COM));
	printk("-----------------dc_reg------------------\n");
}

static void dump_tft_reg(void)
{
	printk("----------------tft_reg------------------\n");
	printk("TFT_TIMING_HSYNC:   %lx\n",reg_read(fbdev, DC_TFT_HSYNC));
	printk("TFT_TIMING_VSYNC:   %lx\n",reg_read(fbdev, DC_TFT_VSYNC));
	printk("TFT_TIMING_HDE:     %lx\n",reg_read(fbdev, DC_TFT_HDE));
	printk("TFT_TIMING_VDE:     %lx\n",reg_read(fbdev, DC_TFT_VDE));
	printk("TFT_TRAN_CFG:       %lx\n",reg_read(fbdev, DC_TFT_CFG));
	printk("TFT_ST:             %lx\n",reg_read(fbdev, DC_TFT_ST));
	printk("----------------tft_reg------------------\n");
}

static void dump_slcd_reg(void)
{
	printk("---------------slcd_reg------------------\n");
	printk("SLCD_CFG:           %lx\n",reg_read(fbdev, DC_SLCD_CFG));
	printk("SLCD_WR_DUTY:       %lx\n",reg_read(fbdev, DC_SLCD_WR_DUTY));
	printk("SLCD_TIMING:        %lx\n",reg_read(fbdev, DC_SLCD_TIMING));
	printk("SLCD_FRM_SIZE:      %lx\n",reg_read(fbdev, DC_SLCD_FRM_SIZE));
	printk("SLCD_SLOW_TIME:     %lx\n",reg_read(fbdev, DC_SLCD_SLOW_TIME));
	printk("SLCD_CMD:           %lx\n",reg_read(fbdev, DC_SLCD_CMD));
	printk("SLCD_ST:            %lx\n",reg_read(fbdev, DC_SLCD_ST));
	printk("---------------slcd_reg------------------\n");
}

static void dump_frm_desc_reg(void)
{
	unsigned int ctrl;
	ctrl = reg_read(fbdev, DC_CTRL);
	ctrl |= DC_DES_CNT_RST;
	reg_write(fbdev, DC_CTRL, ctrl);

	printk("--------Frame Descriptor register--------\n");
	printk("FrameNextCfgAddr:   %lx\n",reg_read(fbdev, DC_FRM_DES));
	printk("FrameSize:          %lx\n",reg_read(fbdev, DC_FRM_DES));
	printk("FrameCtrl:          %lx\n",reg_read(fbdev, DC_FRM_DES));
	printk("WritebackAddr:      %lx\n",reg_read(fbdev, DC_FRM_DES));
	printk("WritebackStride:    %lx\n",reg_read(fbdev, DC_FRM_DES));
	printk("Layer0CfgAddr:      %lx\n",reg_read(fbdev, DC_FRM_DES));
	printk("Layer1CfgAddr:      %lx\n",reg_read(fbdev, DC_FRM_DES));
	printk("Layer2CfgAddr:      %lx\n",reg_read(fbdev, DC_FRM_DES));
	printk("Layer3CfgAddr:      %lx\n",reg_read(fbdev, DC_FRM_DES));
	printk("LayCfgEn:	    %lx\n",reg_read(fbdev, DC_FRM_DES));
	printk("InterruptControl:   %lx\n",reg_read(fbdev, DC_FRM_DES));
	printk("--------Frame Descriptor register--------\n");
}

static void dump_layer_desc_reg(void)
{
	unsigned int ctrl;
	ctrl = reg_read(fbdev, DC_CTRL);
	ctrl |= DC_DES_CNT_RST;
	reg_write(fbdev, DC_CTRL, ctrl);

	printk("--------layer0 Descriptor register-------\n");
	printk("LayerSize:          %lx\n",reg_read(fbdev, DC_LAY0_DES));
	printk("LayerCfg:           %lx\n",reg_read(fbdev, DC_LAY0_DES));
	printk("LayerBufferAddr:    %lx\n",reg_read(fbdev, DC_LAY0_DES));
	printk("LayerScale:         %lx\n",reg_read(fbdev, DC_LAY0_DES));
	printk("LayerRotation:      %lx\n",reg_read(fbdev, DC_LAY0_DES));
	printk("LayerScratch:       %lx\n",reg_read(fbdev, DC_LAY0_DES));
	printk("LayerPos:           %lx\n",reg_read(fbdev, DC_LAY0_DES));
	printk("LayerResizeCoef_X:  %lx\n",reg_read(fbdev, DC_LAY0_DES));
	printk("LayerResizeCoef_Y:  %lx\n",reg_read(fbdev, DC_LAY0_DES));
	printk("LayerStride:        %lx\n",reg_read(fbdev, DC_LAY0_DES));
	printk("--------layer0 Descriptor register-------\n");

	printk("--------layer1 Descriptor register-------\n");
	printk("LayerSize:          %lx\n",reg_read(fbdev, DC_LAY1_DES));
	printk("LayerCfg:           %lx\n",reg_read(fbdev, DC_LAY1_DES));
	printk("LayerBufferAddr:    %lx\n",reg_read(fbdev, DC_LAY1_DES));
	printk("LayerScale:         %lx\n",reg_read(fbdev, DC_LAY1_DES));
	printk("LayerRotation:      %lx\n",reg_read(fbdev, DC_LAY1_DES));
	printk("LayerScratch:       %lx\n",reg_read(fbdev, DC_LAY1_DES));
	printk("LayerPos:           %lx\n",reg_read(fbdev, DC_LAY1_DES));
	printk("LayerResizeCoef_X:  %lx\n",reg_read(fbdev, DC_LAY1_DES));
	printk("LayerResizeCoef_Y:  %lx\n",reg_read(fbdev, DC_LAY1_DES));
	printk("LayerStride:        %lx\n",reg_read(fbdev, DC_LAY1_DES));
	printk("--------layer1 Descriptor register-------\n");
}

static void dump_frm_desc(struct ingenicfb_framedesc *framedesc, int index)
{
	printk("-------User Frame Descriptor index[%d]-----\n", index);
	printk("FramedescAddr:	    0x%x\n",(uint32_t)framedesc);
	printk("FrameNextCfgAddr:   0x%x\n",framedesc->FrameNextCfgAddr);
	printk("FrameSize:          0x%x\n",framedesc->FrameSize.d32);
	printk("FrameCtrl:          0x%x\n",framedesc->FrameCtrl.d32);
	printk("Layer0CfgAddr:      0x%x\n",framedesc->Layer0CfgAddr);
	printk("Layer1CfgAddr:      0x%x\n",framedesc->Layer1CfgAddr);
	printk("LayerCfgEn:	    0x%x\n",framedesc->LayCfgEn.d32);
	printk("InterruptControl:   0x%x\n",framedesc->InterruptControl.d32);
	printk("-------User Frame Descriptor index[%d]-----\n", index);
}

static void dump_layer_desc(struct ingenicfb_layerdesc *layerdesc, int row, int col)
{
	printk("------User layer Descriptor index[%d][%d]------\n", row, col);
	printk("LayerdescAddr:	    0x%x\n",(uint32_t)layerdesc);
	printk("LayerSize:          0x%x\n",layerdesc->LayerSize.d32);
	printk("LayerCfg:           0x%x\n",layerdesc->LayerCfg.d32);
	printk("LayerBufferAddr:    0x%x\n",layerdesc->LayerBufferAddr);
	printk("LayerScale:         0x%x\n",layerdesc->LayerScale.d32);
	printk("LayerPos:           0x%x\n",layerdesc->LayerPos.d32);
	printk("LayerStride:        0x%x\n",layerdesc->LayerStride);
	printk("------User layer Descriptor index[%d][%d]------\n", row, col);
}

void dump_lay_cfg(struct ingenicfb_lay_cfg * lay_cfg, int index)
{
	printk("------User disp set index[%d]------\n", index);
	printk("lay_en:		   0x%x\n",lay_cfg->lay_en);
	printk("lay_z_order:	   0x%x\n",lay_cfg->lay_z_order);
	printk("pic_witdh:	   0x%x\n",lay_cfg->pic_width);
	printk("pic_heght:	   0x%x\n",lay_cfg->pic_height);
	printk("disp_pos_x:	   0x%x\n",lay_cfg->disp_pos_x);
	printk("disp_pos_y:	   0x%x\n",lay_cfg->disp_pos_y);
	printk("g_alpha_en:	   0x%x\n",lay_cfg->g_alpha_en);
	printk("g_alpha_val:	   0x%x\n",lay_cfg->g_alpha_val);
	printk("color:		   0x%x\n",lay_cfg->color);
	printk("format:		   0x%x\n",lay_cfg->format);
	printk("stride:		   0x%x\n",lay_cfg->stride);
	printk("------User disp set index[%d]------\n", index);
}

static void dump_lcdc_registers(void)
{
	dump_dc_reg();
	dump_tft_reg();
	dump_slcd_reg();
	dump_frm_desc_reg();
	dump_layer_desc_reg();
}

static void dump_desc(struct ingenicfb_device *fbdev)
{
	int i, j;
	for(i = 0; i < MAX_DESC_NUM; i++) {
		for(j = 0; j < MAX_LAYER_NUM; j++) {
			dump_layer_desc(fbdev->layerdesc[i][j], i, j);
		}
		dump_frm_desc(fbdev->framedesc[i], i);
	}
}

	static ssize_t
dump_lcd(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ingenicfb_device *fbdev = dev_get_drvdata(dev);
	printk("\nDisp_end_num = %d\n\n", fbdev->irq_cnt);
	printk("\nTFT_UNDR_num = %d\n\n", fbdev->tft_undr_cnt);
	printk("\nFrm_start_num = %d\n\n", fbdev->frm_start);
	printk(" Pand display count=%d\n",fbdev->pan_display_count);
	printk("timestamp.wp = %d , timestamp.rp = %d\n\n", fbdev->timestamp.wp, fbdev->timestamp.rp);
	dump_lcdc_registers();
	dump_desc(fbdev);
	return 0;
}

static void dump_all(struct ingenicfb_device *fbdev)
{
	printk("\ndisp_end_num = %d\n\n", fbdev->irq_cnt);
	printk("\nTFT_UNDR_num = %d\n\n", fbdev->tft_undr_cnt);
	printk("\nFrm_start_num = %d\n\n", fbdev->frm_start);
	dump_lcdc_registers();
	dump_desc(fbdev);
}

void ingenicfb_clk_enable(struct ingenicfb_device *fbdev)
{
	if(fbdev->is_clk_en){
		return;
	}
	clk_prepare_enable(fbdev->pclk);
	clk_prepare_enable(fbdev->clk);
	fbdev->is_clk_en = 1;
}

void ingenicfb_clk_disable(struct ingenicfb_device *fbdev)
{
	if(!fbdev->is_clk_en){
		return;
	}
	fbdev->is_clk_en = 0;
	clk_disable_unprepare(fbdev->clk);
	clk_disable_unprepare(fbdev->pclk);
}

	static void
ingenicfb_videomode_to_var(struct fb_var_screeninfo *var,
		const struct fb_videomode *mode, int lcd_type)
{
	var->xres = mode->xres;
	var->yres = mode->yres;
	var->xres_virtual = mode->xres;
	var->yres_virtual = mode->yres * MAX_DESC_NUM * MAX_LAYER_NUM;
	var->xoffset = 0;
	var->yoffset = 0;
	var->left_margin = mode->left_margin;
	var->right_margin = mode->right_margin;
	var->upper_margin = mode->upper_margin;
	var->lower_margin = mode->lower_margin;
	var->hsync_len = mode->hsync_len;
	var->vsync_len = mode->vsync_len;
	var->sync = mode->sync;
	var->vmode = mode->vmode & FB_VMODE_MASK;
	var->pixclock = mode->pixclock;
}

static struct fb_videomode *ingenicfb_get_mode(struct fb_var_screeninfo *var,
		struct fb_info *info)
{
	size_t i;
	struct ingenicfb_device *fbdev = info->par;
	struct fb_videomode *mode = fbdev->panel->modes;

	for (i = 0; i < fbdev->panel->num_modes; ++i, ++mode) {
		if (mode->xres == var->xres && mode->yres == var->yres
				&& mode->vmode == var->vmode
				&& mode->right_margin == var->right_margin) {
			if (fbdev->panel->lcd_type != LCD_TYPE_SLCD) {
				if (mode->pixclock == var->pixclock)
					return mode;
			} else {
				return mode;
			}
		}
	}

	return NULL;
}

static int ingenicfb_check_frm_cfg(struct fb_info *info, struct ingenicfb_frm_cfg *frm_cfg)
{
	struct fb_var_screeninfo *var = &info->var;
	struct ingenicfb_lay_cfg *lay_cfg;
	struct fb_videomode *mode;

	mode = ingenicfb_get_mode(var, info);
	if (mode == NULL) {
		dev_err(info->dev, "%s get video mode failed\n", __func__);
		return -EINVAL;
	}

	lay_cfg = frm_cfg->lay_cfg;

	if((!lay_cfg[0].lay_en) ||
	   (lay_cfg[0].lay_z_order != 1) ||
	   (lay_cfg[0].lay_z_order == lay_cfg[1].lay_z_order) ||
	   (lay_cfg[0].pic_width > mode->xres) ||
	   (lay_cfg[0].pic_width == 0) ||
	   (lay_cfg[0].pic_height > mode->yres) ||
	   (lay_cfg[0].pic_height == 0) ||
	   (lay_cfg[0].disp_pos_x > mode->xres) ||
	   (lay_cfg[0].disp_pos_y > mode->yres) ||
	   (lay_cfg[0].color > LAYER_CFG_COLOR_BGR) ||
	   (lay_cfg[0].stride > 4096)) {
		dev_err(info->dev,"%s frame[0] cfg value is err!\n",__func__);
		return -EINVAL;
	}

	switch (lay_cfg[0].format) {
	case LAYER_CFG_FORMAT_RGB555:
	case LAYER_CFG_FORMAT_ARGB1555:
	case LAYER_CFG_FORMAT_RGB565:
	case LAYER_CFG_FORMAT_RGB888:
	case LAYER_CFG_FORMAT_ARGB8888:
	case LAYER_CFG_FORMAT_YUV422:
		break;
	default:
		dev_err(info->dev,"%s frame[0] cfg value is err!\n",__func__);
		return -EINVAL;
	}

	if(lay_cfg[1].lay_en) {
		if((lay_cfg[1].lay_z_order != 0) ||
		   (lay_cfg[1].pic_width > mode->xres) ||
		   (lay_cfg[1].pic_width == 0) ||
		   (lay_cfg[1].pic_height > mode->yres) ||
		   (lay_cfg[1].pic_height == 0) ||
		   (lay_cfg[1].disp_pos_x > mode->xres) ||
		   (lay_cfg[1].disp_pos_y > mode->yres) ||
		   (lay_cfg[1].color > LAYER_CFG_COLOR_BGR) ||
		   (lay_cfg[1].stride > 4096)) {
			dev_err(info->dev,"%s frame[1] cfg value is err!\n",__func__);
			return -EINVAL;
		}
		switch (lay_cfg[1].format) {
		case LAYER_CFG_FORMAT_RGB555:
		case LAYER_CFG_FORMAT_ARGB1555:
		case LAYER_CFG_FORMAT_RGB565:
		case LAYER_CFG_FORMAT_RGB888:
		case LAYER_CFG_FORMAT_ARGB8888:
			break;
		default:
			dev_err(info->dev,"%s frame[0] cfg value is err!\n",__func__);
			return -EINVAL;
		}
	}

	return 0;
}

static void ingenicfb_colormode_to_var(struct fb_var_screeninfo *var,
		struct ingenicfb_colormode *color)
{
	var->bits_per_pixel = color->bits_per_pixel;
	var->nonstd = color->nonstd;
	var->red = color->red;
	var->green = color->green;
	var->blue = color->blue;
	var->transp = color->transp;
}

static bool cmp_var_to_colormode(struct fb_var_screeninfo *var,
		struct ingenicfb_colormode *color)
{
	bool cmp_component(struct fb_bitfield *f1, struct fb_bitfield *f2)
	{
		return f1->length == f2->length &&
			f1->offset == f2->offset &&
			f1->msb_right == f2->msb_right;
	}

	if (var->bits_per_pixel == 0 ||
			var->red.length == 0 ||
			var->blue.length == 0 ||
			var->green.length == 0)
		return 0;

	return var->bits_per_pixel == color->bits_per_pixel &&
		cmp_component(&var->red, &color->red) &&
		cmp_component(&var->green, &color->green) &&
		cmp_component(&var->blue, &color->blue) &&
		cmp_component(&var->transp, &color->transp);
}

static int ingenicfb_check_colormode(struct fb_var_screeninfo *var, uint32_t *mode)
{
	int i;

	if (var->nonstd) {
		for (i = 0; i < ARRAY_SIZE(ingenicfb_colormodes); ++i) {
			struct ingenicfb_colormode *m = &ingenicfb_colormodes[i];
			if (var->nonstd == m->nonstd) {
				ingenicfb_colormode_to_var(var, m);
				*mode = m->mode;
				return 0;
			}
		}

		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(ingenicfb_colormodes); ++i) {
		struct ingenicfb_colormode *m = &ingenicfb_colormodes[i];
		if (cmp_var_to_colormode(var, m)) {
			ingenicfb_colormode_to_var(var, m);
			*mode = m->mode;
			return 0;
		}
	}

	return -EINVAL;
}

static int ingenicfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;
	struct fb_videomode *mode;
	uint32_t colormode;
	int ret;

	mode = ingenicfb_get_mode(var, info);
	if (mode == NULL) {
		dev_err(info->dev, "%s get video mode failed\n", __func__);
		return -EINVAL;
	}

	ingenicfb_videomode_to_var(var, mode, fbdev->panel->lcd_type);

	ret = ingenicfb_check_colormode(var, &colormode);
	if(ret) {
		dev_err(info->dev,"Check colormode failed!\n");
		return  ret;
	}

	return 0;
}

static void slcd_send_mcu_command(struct ingenicfb_device *fbdev, unsigned long cmd)
{
	int count = 10000;
	uint32_t slcd_cfg;

	while ((reg_read(fbdev, DC_SLCD_ST) & DC_SLCD_ST_BUSY) && count--) {
		udelay(10);
	}
	if (count < 0) {
		dev_err(fbdev->dev, "SLCDC wait busy state wrong");
	}

	slcd_cfg = reg_read(fbdev, DC_SLCD_CFG);
/*	reg_write(fbdev, DC_SLCD_CFG, (slcd_cfg & ~DC_FMT_EN));*/     //notice
	reg_write(fbdev, DC_SLCD_CMD, DC_SLCD_CMD_FLAG_CMD | (cmd & ~DC_SLCD_CMD_FLAG_MASK));
}

static void slcd_send_mcu_data(struct ingenicfb_device *fbdev, unsigned long data)
{
	int count = 10000;
	uint32_t slcd_cfg;

	while ((reg_read(fbdev, DC_SLCD_ST) & DC_SLCD_ST_BUSY) && count--) {
		udelay(10);
	}
	if (count < 0) {
		dev_err(fbdev->dev, "SLCDC wait busy state wrong");
	}

	slcd_cfg = reg_read(fbdev, DC_SLCD_CFG);
	reg_write(fbdev, DC_SLCD_CFG, (slcd_cfg | DC_FMT_EN));
	reg_write(fbdev, DC_SLCD_CMD, DC_SLCD_CMD_FLAG_DATA | (data & ~DC_SLCD_CMD_FLAG_MASK));
}

static void slcd_send_mcu_prm(struct ingenicfb_device *fbdev, unsigned long data)
{
	int count = 10000;
	uint32_t slcd_cfg;

	while ((reg_read(fbdev, DC_SLCD_ST) & DC_SLCD_ST_BUSY) && count--) {
		udelay(10);
	}
	if (count < 0) {
		dev_err(fbdev->dev, "SLCDC wait busy state wrong");
	}

	slcd_cfg = reg_read(fbdev, DC_SLCD_CFG);
/*	reg_write(fbdev, DC_SLCD_CFG, (slcd_cfg & ~DC_FMT_EN));*/    //notice
	reg_write(fbdev, DC_SLCD_CMD, DC_SLCD_CMD_FLAG_PRM | (data & ~DC_SLCD_CMD_FLAG_MASK));
}

static void wait_slcd_busy(void)
{
	int count = 100000;
	while ((reg_read(fbdev, DC_SLCD_ST) & DC_SLCD_ST_BUSY)
			&& count--) {
		udelay(10);
	}
	if (count < 0) {
		dev_err(fbdev->dev,"SLCDC wait busy state wrong");
	}
}

static int wait_dc_state(uint32_t state, uint32_t flag)
{
	unsigned long timeout = 20000;
	while(((!(reg_read(fbdev, DC_ST) & state)) == flag) && timeout) {
		timeout--;
		udelay(10);
	}
	if(timeout <= 0) {
		printk("LCD wait state timeout! state = %d, DC_ST = 0x%x\n", state, DC_ST);
		return -1;
	}
	return 0;
}

static void ingenicfb_slcd_mcu_init(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;
	struct lcd_panel *panel = fbdev->panel;
	struct smart_config *smart_config;
	struct smart_lcd_data_table *data_table;
	uint32_t length_data_table;
	uint32_t i;

	smart_config = panel->smart_config;
	data_table = smart_config->data_table;
	length_data_table = smart_config->length_data_table;
	if (panel->lcd_type != LCD_TYPE_SLCD)
		return;

	if(length_data_table && data_table) {
		for(i = 0; i < length_data_table; i++) {
			switch (data_table[i].type) {
			case SMART_CONFIG_DATA:
				slcd_send_mcu_data(fbdev, data_table[i].value);
				break;
			case SMART_CONFIG_PRM:
				slcd_send_mcu_prm(fbdev, data_table[i].value);
				break;
			case SMART_CONFIG_CMD:
				slcd_send_mcu_command(fbdev, data_table[i].value);
				break;
			case SMART_CONFIG_UDELAY:
				udelay(data_table[i].value);
				break;
			default:
				printk("Unknow SLCD data type\n");
				break;
			}
		}
	}
}

static void ingenicfb_cmp_start(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;

	if(!(reg_read(fbdev, DC_ST) & DC_FRM_WORKING)) {
		reg_write(fbdev, DC_FRM_CFG_CTRL, DC_FRM_START);
	} else {
		//dev_err(fbdev->dev, "Composer has enabled.\n");
	}
}

static void ingenicfb_tft_start(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;

	if(!(reg_read(fbdev, DC_TFT_ST) & DC_TFT_WORKING)) {
		reg_write(fbdev, DC_CTRL, DC_TFT_START);
	} else {
		dev_err(fbdev->dev, "TFT has enabled.\n");
	}
}

static void ingenicfb_slcd_start(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;

	if(!(reg_read(fbdev, DC_SLCD_ST) & DC_SLCD_ST_BUSY)) {
		reg_write(fbdev, DC_CTRL, DC_SLCD_START);
	} else {
		dev_err(fbdev->dev, "Slcd has enabled.\n");
	}
}

static void ingenicfb_gen_stp_cmp(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;

	reg_write(fbdev, DC_CTRL, DC_GEN_STP_CMP);
}

static void ingenicfb_qck_stp_cmp(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;

	reg_write(fbdev, DC_CTRL, DC_QCK_STP_CMP);
}

static void tft_timing_init(struct fb_videomode *modes) {
	uint32_t hps;
	uint32_t hpe;
	uint32_t vps;
	uint32_t vpe;
	uint32_t hds;
	uint32_t hde;
	uint32_t vds;
	uint32_t vde;

	hps = modes->hsync_len;
	hpe = hps + modes->left_margin + modes->xres + modes->right_margin;
	vps = modes->vsync_len;
	vpe = vps + modes->upper_margin + modes->yres + modes->lower_margin;

	hds = modes->hsync_len + modes->left_margin;
	hde = hds + modes->xres;
	vds = modes->vsync_len + modes->upper_margin;
	vde = vds + modes->yres;

	reg_write(fbdev, DC_TFT_HSYNC,
		  (hps << DC_HPS_LBIT) |
		  (hpe << DC_HPE_LBIT));
	reg_write(fbdev, DC_TFT_VSYNC,
		  (vps << DC_VPS_LBIT) |
		  (vpe << DC_VPE_LBIT));
	reg_write(fbdev, DC_TFT_HDE,
		  (hds << DC_HDS_LBIT) |
		  (hde << DC_HDE_LBIT));
	reg_write(fbdev, DC_TFT_VDE,
		  (vds << DC_VDS_LBIT) |
		  (vde << DC_VDE_LBIT));
}

void tft_cfg_init(struct tft_config *tft_config) {
	uint32_t tft_cfg;

	tft_cfg = reg_read(fbdev, DC_TFT_CFG);
	if(tft_config->pix_clk_inv) {
		tft_cfg |= DC_PIX_CLK_INV;
	} else {
		tft_cfg &= ~DC_PIX_CLK_INV;
	}

	if(tft_config->de_dl) {
		tft_cfg |= DC_DE_DL;
	} else {
		tft_cfg &= ~DC_DE_DL;
	}

	if(tft_config->sync_dl) {
		tft_cfg |= DC_SYNC_DL;
	} else {
		tft_cfg &= ~DC_SYNC_DL;
	}

	tft_cfg &= ~DC_COLOR_EVEN_MASK;
	switch(tft_config->color_even) {
	case TFT_LCD_COLOR_EVEN_RGB:
		tft_cfg |= DC_EVEN_RGB;
		break;
	case TFT_LCD_COLOR_EVEN_RBG:
		tft_cfg |= DC_EVEN_RBG;
		break;
	case TFT_LCD_COLOR_EVEN_BGR:
		tft_cfg |= DC_EVEN_BGR;
		break;
	case TFT_LCD_COLOR_EVEN_BRG:
		tft_cfg |= DC_EVEN_BRG;
		break;
	case TFT_LCD_COLOR_EVEN_GBR:
		tft_cfg |= DC_EVEN_GBR;
		break;
	case TFT_LCD_COLOR_EVEN_GRB:
		tft_cfg |= DC_EVEN_GRB;
		break;
	default:
		printk("err!\n");
		break;
	}

	tft_cfg &= ~DC_COLOR_ODD_MASK;
	switch(tft_config->color_odd) {
	case TFT_LCD_COLOR_ODD_RGB:
		tft_cfg |= DC_ODD_RGB;
		break;
	case TFT_LCD_COLOR_ODD_RBG:
		tft_cfg |= DC_ODD_RBG;
		break;
	case TFT_LCD_COLOR_ODD_BGR:
		tft_cfg |= DC_ODD_BGR;
		break;
	case TFT_LCD_COLOR_ODD_BRG:
		tft_cfg |= DC_ODD_BRG;
		break;
	case TFT_LCD_COLOR_ODD_GBR:
		tft_cfg |= DC_ODD_GBR;
		break;
	case TFT_LCD_COLOR_ODD_GRB:
		tft_cfg |= DC_ODD_GRB;
		break;
	default:
		printk("err!\n");
		break;
	}

	tft_cfg &= ~DC_MODE_MASK;
	switch(tft_config->mode) {
	case TFT_LCD_MODE_PARALLEL_888:
		tft_cfg |= DC_MODE_PARALLEL_888;
		break;
	case TFT_LCD_MODE_PARALLEL_666:
		tft_cfg |= DC_MODE_PARALLEL_666;
		break;
	case TFT_LCD_MODE_PARALLEL_565:
		tft_cfg |= DC_MODE_PARALLEL_565;
		break;
	case TFT_LCD_MODE_SERIAL_RGB:
		tft_cfg |= DC_MODE_SERIAL_8BIT_RGB;
		break;
	case TFT_LCD_MODE_SERIAL_RGBD:
		tft_cfg |= DC_MODE_SERIAL_8BIT_RGBD;
		break;
	default:
		printk("err!\n");
		break;
	}
	reg_write(fbdev, DC_TFT_CFG, tft_cfg);
}

static int ingenicfb_tft_set_par(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;
	struct lcd_panel *panel = fbdev->panel;
	struct lcd_panel_ops *panel_ops;
	struct fb_videomode *mode;

	panel_ops = panel->ops;

	mode = ingenicfb_get_mode(&info->var, info);
	if (mode == NULL) {
		dev_err(info->dev, "%s get video mode failed\n", __func__);
		return -EINVAL;
	}

	tft_timing_init(mode);
	tft_cfg_init(panel->tft_config);
	if(panel_ops && panel_ops->enable)
		panel_ops->enable(panel);

	return 0;
}

static void slcd_cfg_init(struct smart_config *smart_config) {
	uint32_t slcd_cfg;

	slcd_cfg = reg_read(fbdev, DC_SLCD_CFG);

	slcd_cfg |= DC_FRM_MD;

	if(smart_config->rdy_switch) {
		slcd_cfg |= DC_RDY_SWITCH;

		if(smart_config->rdy_dp) {
			slcd_cfg |= DC_RDY_DP;
		} else {
			slcd_cfg &= ~DC_RDY_DP;
		}
		if(smart_config->rdy_anti_jit) {
			slcd_cfg |= DC_RDY_ANTI_JIT;
		} else {
			slcd_cfg &= ~DC_RDY_ANTI_JIT;
		}
	} else {
		slcd_cfg &= ~DC_RDY_SWITCH;
	}

	if(smart_config->te_switch) {
		slcd_cfg |= DC_TE_SWITCH;

		if(smart_config->te_dp) {
			slcd_cfg |= DC_TE_DP;
		} else {
			slcd_cfg &= ~DC_TE_DP;
		}
		if(smart_config->te_md) {
			slcd_cfg |= DC_TE_MD;
		} else {
			slcd_cfg &= ~DC_TE_MD;
		}
		if(smart_config->te_anti_jit) {
			slcd_cfg |= DC_TE_ANTI_JIT;
		} else {
			slcd_cfg &= ~DC_TE_ANTI_JIT;
		}
	} else {
		slcd_cfg &= ~DC_TE_SWITCH;
	}

	if(smart_config->cs_en) {
		slcd_cfg |= DC_CS_EN;

		if(smart_config->cs_dp) {
			slcd_cfg |= DC_CS_DP;
		} else {
			slcd_cfg &= ~DC_CS_DP;
		}
	} else {
		slcd_cfg &= ~DC_CS_EN;
	}

	if(smart_config->dc_md) {
		slcd_cfg |= DC_DC_MD;
	} else {
		slcd_cfg &= ~DC_DC_MD;
	}

	if(smart_config->wr_md) {
		slcd_cfg |= DC_WR_DP;
	} else {
		slcd_cfg &= ~DC_WR_DP;
	}

	slcd_cfg &= ~DC_DBI_TYPE_MASK;
	switch(smart_config->smart_type){
	case SMART_LCD_TYPE_8080:
		slcd_cfg |= DC_DBI_TYPE_B_8080;
		break;
	case SMART_LCD_TYPE_6800:
		slcd_cfg |= DC_DBI_TYPE_A_6800;
		break;
	case SMART_LCD_TYPE_SPI_3:
		slcd_cfg |= DC_DBI_TYPE_C_SPI_3;
		break;
	case SMART_LCD_TYPE_SPI_4:
		slcd_cfg |= DC_DBI_TYPE_C_SPI_4;
		break;
	default:
		printk("err!\n");
		break;
	}

	slcd_cfg &= ~DC_DATA_FMT_MASK;
	switch(smart_config->pix_fmt) {
	case SMART_LCD_FORMAT_888:
		slcd_cfg |= DC_DATA_FMT_888;
		break;
	case SMART_LCD_FORMAT_666:
		slcd_cfg |= DC_DATA_FMT_666;
		break;
	case SMART_LCD_FORMAT_565:
		slcd_cfg |= DC_DATA_FMT_565;
		break;
	default:
		printk("err!\n");
		break;
	}

	slcd_cfg &= ~DC_DWIDTH_MASK;
	switch(smart_config->dwidth) {
	case SMART_LCD_DWIDTH_8_BIT:
		slcd_cfg |= DC_DWIDTH_8BITS;
		break;
	case SMART_LCD_DWIDTH_9_BIT:
		slcd_cfg |= DC_DWIDTH_9BITS;
		break;
	case SMART_LCD_DWIDTH_16_BIT:
		slcd_cfg |= DC_DWIDTH_16BITS;
		break;
	case SMART_LCD_DWIDTH_18_BIT:
		slcd_cfg |= DC_DWIDTH_18BITS;
		break;
	case SMART_LCD_DWIDTH_24_BIT:
		slcd_cfg |= DC_DWIDTH_24BITS;
		break;
	default:
		printk("err!\n");
		break;
	}

	slcd_cfg &= ~DC_CWIDTH_MASK;
	switch(smart_config->cwidth) {
	case SMART_LCD_CWIDTH_8_BIT:
		slcd_cfg |= DC_CWIDTH_8BITS;
		break;
	case SMART_LCD_CWIDTH_9_BIT:
		slcd_cfg |= DC_CWIDTH_9BITS;
		break;
	case SMART_LCD_CWIDTH_16_BIT:
		slcd_cfg |= DC_CWIDTH_16BITS;
		break;
	case SMART_LCD_CWIDTH_18_BIT:
		slcd_cfg |= DC_CWIDTH_18BITS;
		break;
	case SMART_LCD_CWIDTH_24_BIT:
		slcd_cfg |= DC_CWIDTH_24BITS;
		break;
	default:
		printk("err!\n");
		break;
	}

	reg_write(fbdev, DC_SLCD_CFG, slcd_cfg);

	return;
}

static int slcd_timing_init(struct lcd_panel *ingenicfb_panel)
{
	uint32_t width = ingenicfb_panel->width;
	uint32_t height = ingenicfb_panel->height;
	uint32_t dhtime = 0;
	uint32_t dltime = 0;
	uint32_t chtime = 0;
	uint32_t cltime = 0;
	uint32_t tah = 0;
	uint32_t tas = 0;
	uint32_t slowtime = 0;

	/*frm_size*/
	reg_write(fbdev, DC_SLCD_FRM_SIZE,
		  ((width << DC_SLCD_FRM_H_SIZE_LBIT) |
		   (height << DC_SLCD_FRM_V_SIZE_LBIT)));

	/* wr duty */
	reg_write(fbdev, DC_SLCD_WR_DUTY,
		  ((dhtime << DC_DSTIME_LBIT) |
		   (dltime << DC_DDTIME_LBIT) |
		   (chtime << DC_CSTIME_LBIT) |
		   (cltime << DC_CDTIME_LBIT)));

	/* slcd timing */
	reg_write(fbdev, DC_SLCD_TIMING,
		  ((tah << DC_TAH_LBIT) |
		  (tas << DC_TAS_LBIT)));

	/* slow time */
	reg_write(fbdev, DC_SLCD_SLOW_TIME, slowtime);

	return 0;
}

static int ingenicfb_slcd_set_par(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;
	struct lcd_panel *panel = fbdev->panel;
	struct lcd_panel_ops *panel_ops;

	panel_ops = panel->ops;

	slcd_cfg_init(panel->smart_config);
	slcd_timing_init(panel);

	if(panel_ops && panel_ops->enable)
		    panel_ops->enable(panel);

	ingenicfb_slcd_mcu_init(info);

	return 0;
}

static void csc_mode_set(struct ingenicfb_device *fbdev, csc_mode_t mode) {
	switch(mode) {
	case CSC_MODE_0:
		reg_write(fbdev, DC_CSC_MULT_YRV, DC_CSC_MULT_Y_MD0 | DC_CSC_MULT_RV_MD0);
		reg_write(fbdev, DC_CSC_MULT_GUGV, DC_CSC_MULT_GU_MD0 | DC_CSC_MULT_GV_MD0);
		reg_write(fbdev, DC_CSC_MULT_BU, DC_CSC_MULT_BU_MD0);
		reg_write(fbdev, DC_CSC_SUB_YUV, DC_CSC_SUB_Y_MD0 | DC_CSC_SUB_UV_MD0);
		break;
	case CSC_MODE_1:
		reg_write(fbdev, DC_CSC_MULT_YRV, DC_CSC_MULT_Y_MD1 | DC_CSC_MULT_RV_MD1);
		reg_write(fbdev, DC_CSC_MULT_GUGV, DC_CSC_MULT_GU_MD1 | DC_CSC_MULT_GV_MD1);
		reg_write(fbdev, DC_CSC_MULT_BU, DC_CSC_MULT_BU_MD1);
		reg_write(fbdev, DC_CSC_SUB_YUV, DC_CSC_SUB_Y_MD1 | DC_CSC_SUB_UV_MD1);
		break;
	case CSC_MODE_2:
		reg_write(fbdev, DC_CSC_MULT_YRV, DC_CSC_MULT_Y_MD2 | DC_CSC_MULT_RV_MD2);
		reg_write(fbdev, DC_CSC_MULT_GUGV, DC_CSC_MULT_GU_MD2 | DC_CSC_MULT_GV_MD2);
		reg_write(fbdev, DC_CSC_MULT_BU, DC_CSC_MULT_BU_MD2);
		reg_write(fbdev, DC_CSC_SUB_YUV, DC_CSC_SUB_Y_MD2 | DC_CSC_SUB_UV_MD2);
		break;
	case CSC_MODE_3:
		reg_write(fbdev, DC_CSC_MULT_YRV, DC_CSC_MULT_Y_MD3 | DC_CSC_MULT_RV_MD3);
		reg_write(fbdev, DC_CSC_MULT_GUGV, DC_CSC_MULT_GU_MD3 | DC_CSC_MULT_GV_MD3);
		reg_write(fbdev, DC_CSC_MULT_BU, DC_CSC_MULT_BU_MD3);
		reg_write(fbdev, DC_CSC_SUB_YUV, DC_CSC_SUB_Y_MD3 | DC_CSC_SUB_UV_MD3);
		break;
	default:
		dev_err(fbdev->dev, "Set csc mode err!\n");
		break;
	}
}

static int ingenicfb_alloc_devmem(struct ingenicfb_device *fbdev)
{
	struct fb_videomode *mode;
	struct fb_info *fb = fbdev->fb;
	uint32_t buff_size;
	void *page;
	uint8_t *addr;
	dma_addr_t addr_phy;
	int i, j;

	mode = fbdev->panel->modes;
	if (!mode) {
		dev_err(fbdev->dev, "Checkout video mode fail\n");
		return -EINVAL;
	}

	buff_size = sizeof(struct ingenicfb_framedesc);
	buff_size = ALIGN(buff_size, DESC_ALIGN);
	addr = dma_alloc_coherent(fbdev->dev, buff_size * MAX_DESC_NUM,
				  &addr_phy, GFP_KERNEL);
	if(addr == NULL) {
		return -ENOMEM;
	}
	for(i = 0; i < MAX_DESC_NUM; i++) {
		fbdev->framedesc[i] =
			(struct ingenicfb_framedesc *)(addr + i * buff_size);
		fbdev->framedesc_phys[i] = addr_phy + i * buff_size;
	}

	buff_size = sizeof(struct ingenicfb_layerdesc);
	buff_size = ALIGN(buff_size, DESC_ALIGN);
	addr = dma_alloc_coherent(fbdev->dev, buff_size * MAX_DESC_NUM * MAX_LAYER_NUM,
				  &addr_phy, GFP_KERNEL);
	if(addr == NULL) {
		return -ENOMEM;
	}
	for(j = 0; j < MAX_DESC_NUM; j++) {
		for(i = 0; i < MAX_LAYER_NUM; i++) {
			fbdev->layerdesc[j][i] = (struct ingenicfb_layerdesc *)
				(addr + i * buff_size + j * buff_size * MAX_LAYER_NUM);
			fbdev->layerdesc_phys[j][i] =
				addr_phy + i * buff_size + j * buff_size * MAX_LAYER_NUM;
		}
	}

	buff_size = mode->xres * mode->yres;
	fbdev->frm_size = buff_size * fb->var.bits_per_pixel >> 3;
	buff_size *= MAX_BITS_PER_PIX >> 3;

	buff_size = buff_size *  MAX_DESC_NUM * MAX_LAYER_NUM;
	fbdev->vidmem_size = PAGE_ALIGN(buff_size);

	fbdev->vidmem[0][0] = dma_alloc_coherent(fbdev->dev, fbdev->vidmem_size,
					  &fbdev->vidmem_phys[0][0], GFP_KERNEL);
	if(fbdev->vidmem[0][0] == NULL) {
		return -ENOMEM;
	}
	for (page = fbdev->vidmem[0][0];
	     page < fbdev->vidmem[0][0] + PAGE_ALIGN(fbdev->vidmem_size);
	     page += PAGE_SIZE) {
		SetPageReserved(virt_to_page(page));
	}

	return 0;
}

#define DPU_WAIT_IRQ_TIME 2000
static int ingenicfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct ingenicfb_device *fbdev = info->par;
	int ret;
	csc_mode_t csc_mode;
	int value;
	int tmp;
	int i;

	switch (cmd) {
		case JZFB_CMP_START:
			ingenicfb_cmp_start(info);
			break;
		case JZFB_TFT_START:
			ingenicfb_tft_start(info);
			break;
		case JZFB_SLCD_START:
			slcd_send_mcu_command(fbdev, fbdev->panel->smart_config->write_gram_cmd);
			wait_slcd_busy();
			ingenicfb_slcd_start(info);
			break;
		case JZFB_GEN_STP_CMP:
			ingenicfb_gen_stp_cmp(info);
			wait_dc_state(DC_WORKING, 0);
			break;
		case JZFB_QCK_STP_CMP:
			ingenicfb_qck_stp_cmp(info);
			wait_dc_state(DC_WORKING, 0);
			break;
		case JZFB_DUMP_LCDC_REG:
			dump_all(info->par);
			break;
		case JZFB_SET_VSYNCINT:
			if (unlikely(copy_from_user(&value, argp, sizeof(int))))
				return -EFAULT;
			tmp = reg_read(fbdev, DC_INTC);
			if (value) {
				if(fbdev->panel->lcd_type == LCD_TYPE_SLCD &&
						!fbdev->slcd_continua) {
					reg_write(fbdev, DC_CLR_ST, DC_CLR_DISP_END);
					reg_write(fbdev, DC_INTC, tmp | DC_EOD_MSK);
				} else {
					reg_write(fbdev, DC_CLR_ST, DC_CLR_FRM_START);
					reg_write(fbdev, DC_INTC, tmp | DC_SOF_MSK);
				}
			} else {
				if(fbdev->panel->lcd_type == LCD_TYPE_SLCD &&
						!fbdev->slcd_continua) {
					reg_write(fbdev, DC_INTC, tmp & ~DC_EOD_MSK);
				} else {
					reg_write(fbdev, DC_INTC, tmp & ~DC_SOF_MSK);
				}
			}
			break;
		case FBIO_WAITFORVSYNC:
			unlock_fb_info(info);
			ret = wait_event_interruptible_timeout(fbdev->vsync_wq,
					fbdev->timestamp.wp != fbdev->timestamp.rp,
					msecs_to_jiffies(DPU_WAIT_IRQ_TIME));
			lock_fb_info(info);
			if(ret == 0) {
				dev_err(info->dev, "DPU wait vsync timeout!\n");
				return -EFAULT;
			}

			ret = copy_to_user(argp, fbdev->timestamp.value + fbdev->timestamp.rp,
					sizeof(u64));
			fbdev->timestamp.rp = (fbdev->timestamp.rp + 1) % TIMESTAMP_CAP;

			if (unlikely(ret))
				return -EFAULT;
			break;
		case JZFB_PUT_FRM_CFG:
			ret = ingenicfb_check_frm_cfg(info, (struct ingenicfb_frm_cfg *)argp);
			if(ret) {
				return ret;
			}
			copy_from_user(&fbdev->current_frm_mode.frm_cfg,
				       (void *)argp,
				       sizeof(struct ingenicfb_frm_cfg));

			for (i = 0; i < ARRAY_SIZE(ingenicfb_colormodes); ++i) {
				struct ingenicfb_colormode *m = &ingenicfb_colormodes[i];
				if (m->mode == fbdev->current_frm_mode.frm_cfg.lay_cfg[0].format) {
					ingenicfb_colormode_to_var(&info->var, m);
					break;
				}
			}

			for(i = 0; i < MAX_DESC_NUM; i++) {
				fbdev->current_frm_mode.update_st[i] = FRAME_CFG_NO_UPDATE;
			}
			break;
		case JZFB_GET_FRM_CFG:
			copy_to_user((void *)argp,
				      &fbdev->current_frm_mode.frm_cfg,
				      sizeof(struct ingenicfb_frm_cfg));
			break;
		case JZFB_SET_CSC_MODE:
			if (unlikely(copy_from_user(&csc_mode, argp, sizeof(csc_mode_t))))
				return -EFAULT;
			csc_mode_set(fbdev, csc_mode);
			break;
		default:
			printk("Command:%x Error!\n",cmd);
			break;
	}
	return 0;
}

static int ingenicfb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct ingenicfb_device *fbdev = info->par;
	unsigned long start;
	unsigned long off;
	u32 len;

	off = vma->vm_pgoff << PAGE_SHIFT;

	/* frame buffer memory */
	start = fbdev->fb->fix.smem_start;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + fbdev->fb->fix.smem_len);
	start &= PAGE_MASK;

	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;
	off += start;

	vma->vm_pgoff = off >> PAGE_SHIFT;
	vma->vm_flags |= VM_IO;

	pgprot_val(vma->vm_page_prot) &= ~_CACHE_MASK;
	/* Write-Acceleration */
	pgprot_val(vma->vm_page_prot) |= _CACHE_CACHABLE_WA;

	if (io_remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
				vma->vm_end - vma->vm_start, vma->vm_page_prot))
	{
		return -EAGAIN;
	}

	return 0;
}

static void ingenicfb_set_vsync_value(struct ingenicfb_device *ingenicfb)
{
	fbdev->vsync_skip_map = (fbdev->vsync_skip_map >> 1 |
				fbdev->vsync_skip_map << 9) & 0x3ff;
	if(likely(fbdev->vsync_skip_map & 0x1)) {
		fbdev->timestamp.value[fbdev->timestamp.wp] =
			ktime_to_ns(ktime_get());
		fbdev->timestamp.wp = (fbdev->timestamp.wp + 1) % TIMESTAMP_CAP;
		wake_up_interruptible(&fbdev->vsync_wq);
	}
}

static int ingenicfb_update_frm_msg(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;
	struct ingenicfb_frmdesc_msg *desc_msg_done;
	struct ingenicfb_frmdesc_msg *desc_msg_reday;

	if(list_empty(&fbdev->desc_run_list)) {
		printk(KERN_DEBUG "%s %d dpu irq run list empty\n", __func__, __LINE__);
		return 0;
	}
	desc_msg_done = list_first_entry(&fbdev->desc_run_list,
			struct ingenicfb_frmdesc_msg, list);
	if(desc_msg_done->state != DESC_ST_RUNING) {
		dev_err(info->dev, "%s Desc state is err!!\n", __func__);
	}

	list_del(&desc_msg_done->list);

	fbdev->frmdesc_msg[desc_msg_done->index].state = DESC_ST_AVAILABLE;
	fbdev->vsync_skip_map = (fbdev->vsync_skip_map >> 1 |
				fbdev->vsync_skip_map << 9) & 0x3ff;
	if(likely(fbdev->vsync_skip_map & 0x1)) {
		fbdev->timestamp.value[fbdev->timestamp.wp] =
			ktime_to_ns(ktime_get());
		fbdev->timestamp.wp = (fbdev->timestamp.wp + 1) % TIMESTAMP_CAP;
		wake_up_interruptible(&fbdev->vsync_wq);
	}

	if(!(reg_read(fbdev, DC_ST) & DC_WORKING)
			&& !(list_empty(&fbdev->desc_run_list))) {
		desc_msg_reday = list_first_entry(&fbdev->desc_run_list,
				struct ingenicfb_frmdesc_msg, list);
		reg_write(fbdev, DC_FRM_CFG_ADDR, desc_msg_reday->addr_phy);
		ingenicfb_cmp_start(info);
		ingenicfb_slcd_start(info);
	}

	return 0;
}

static irqreturn_t ingenicfb_irq_handler(int irq, void *data)
{
	unsigned int irq_flag;
	struct ingenicfb_device *fbdev = (struct ingenicfb_device *)data;

	spin_lock(&fbdev->irq_lock);
	irq_flag = reg_read(fbdev, DC_INT_FLAG);
	if(likely(irq_flag & DC_ST_FRM_START)) {
		reg_write(fbdev, DC_CLR_ST, DC_CLR_FRM_START);
		fbdev->frm_start++;
		if((fbdev->panel->lcd_type == LCD_TYPE_SLCD && fbdev->slcd_continua) ||
				fbdev->panel->lcd_type == LCD_TYPE_TFT) {
			ingenicfb_set_vsync_value(fbdev);
		}
		spin_unlock(&fbdev->irq_lock);
		return IRQ_HANDLED;
	}

	if(likely(irq_flag & DC_DISP_END)) {
		reg_write(fbdev, DC_CLR_ST, DC_CLR_DISP_END);
		fbdev->irq_cnt++;
		if(fbdev->panel->lcd_type == LCD_TYPE_SLCD && !fbdev->slcd_continua) {
			ingenicfb_update_frm_msg(fbdev->fb);
		}
		spin_unlock(&fbdev->irq_lock);
		return IRQ_HANDLED;
	}

	if(unlikely(irq_flag & DC_STOP_DISP_ACK)) {
		reg_write(fbdev, DC_CLR_ST, DC_CLR_STOP_DISP_ACK);
		wake_up_interruptible(&fbdev->gen_stop_wq);
		spin_unlock(&fbdev->irq_lock);
		return IRQ_HANDLED;
	}

	if(unlikely(irq_flag & DC_TFT_UNDR)) {
		reg_write(fbdev, DC_CLR_ST, DC_CLR_TFT_UNDR);
		fbdev->tft_undr_cnt++;
#ifdef CONFIG_FPGA_TEST
		if (!(fbdev->tft_undr_cnt % 100000)) //FIXME:
#endif
			printk("\nTFT_UNDR_num = %d\n\n", fbdev->tft_undr_cnt);
		spin_unlock(&fbdev->irq_lock);
		return IRQ_HANDLED;
	}

	dev_err(fbdev->dev, "DPU irq nothing do, please check!!!\n");
	spin_unlock(&fbdev->irq_lock);
	return IRQ_HANDLED;
}

static inline uint32_t convert_color_to_hw(unsigned val, struct fb_bitfield *bf)
{
	return (((val << bf->length) + 0x7FFF - val) >> 16) << bf->offset;
}

static int ingenicfb_setcolreg(unsigned regno, unsigned red, unsigned green,
		unsigned blue, unsigned transp, struct fb_info *fb)
{
	if (regno >= 16)
		return -EINVAL;

	((uint32_t *)(fb->pseudo_palette))[regno] =
		convert_color_to_hw(red, &fb->var.red) |
		convert_color_to_hw(green, &fb->var.green) |
		convert_color_to_hw(blue, &fb->var.blue) |
		convert_color_to_hw(transp, &fb->var.transp);

	return 0;
}

static void ingenicfb_display_v_color_bar(struct fb_info *info)
{
	int i, j;
	int w, h;
	int bpp;
	unsigned short *p16;
	unsigned int *p32;
	struct ingenicfb_device *fbdev = info->par;
	struct fb_videomode *mode = fbdev->panel->modes;

	if (!mode) {
		dev_err(fbdev->dev, "%s, video mode is NULL\n", __func__);
		return;
	}
	if (!fbdev->vidmem_phys[fbdev->current_frm_desc][0]) {
		dev_err(fbdev->dev, "Not allocate frame buffer yet\n");
		return;
	}
	if (!fbdev->vidmem[fbdev->current_frm_desc][0])
		fbdev->vidmem[fbdev->current_frm_desc][0] =
			(void *)phys_to_virt(fbdev->vidmem_phys[fbdev->current_frm_desc][0]);
	p16 = (unsigned short *)fbdev->vidmem[fbdev->current_frm_desc][0];
	p32 = (unsigned int *)fbdev->vidmem[fbdev->current_frm_desc][0];
	w = mode->xres;
	h = mode->yres;
	bpp = info->var.bits_per_pixel;

	dev_info(info->dev,
			"LCD V COLOR BAR w,h,bpp(%d,%d,%d) fbdev->vidmem[0]=%p\n", w, h,
			bpp, fbdev->vidmem[fbdev->current_frm_desc][0]);

	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			short c16;
			int c32 = 0;
			switch ((j / 10) % 4) {
				case 0:
					c16 = 0xF800;
					c32 = 0xFFFF0000;
					break;
				case 1:
					c16 = 0x07C0;
					c32 = 0xFF00FF00;
					break;
				case 2:
					c16 = 0x001F;
					c32 = 0xFF0000FF;
					break;
				default:
					c16 = 0xFFFF;
					c32 = 0xFFFFFFFF;
					break;
			}
			switch (bpp) {
				case 18:
				case 24:
				case 32:
					*p32++ = c32;
					break;
				default:
					*p16++ = c16;
			}
		}
		if (w % PIXEL_ALIGN) {
			switch (bpp) {
				case 18:
				case 24:
				case 32:
					p32 += (ALIGN(mode->xres, PIXEL_ALIGN) - w);
					break;
				default:
					p16 += (ALIGN(mode->xres, PIXEL_ALIGN) - w);
					break;
			}
		}
	}
}

static void ingenicfb_display_h_color_bar(struct fb_info *info)
{
	int i, j;
	int w, h;
	int bpp;
	unsigned short *p16;
	unsigned int *p32;
	struct ingenicfb_device *fbdev = info->par;
	struct fb_videomode *mode = fbdev->panel->modes;

	if (!mode) {
		dev_err(fbdev->dev, "%s, video mode is NULL\n", __func__);
		return;
	}
	if (!fbdev->vidmem_phys[fbdev->current_frm_desc][0]) {
		dev_err(fbdev->dev, "Not allocate frame buffer yet\n");
		return;
	}
	if (!fbdev->vidmem[fbdev->current_frm_desc][0])
		fbdev->vidmem[fbdev->current_frm_desc][0] =
			(void *)phys_to_virt(fbdev->vidmem_phys[fbdev->current_frm_desc][0]);
	p16 = (unsigned short *)fbdev->vidmem[fbdev->current_frm_desc][0];
	p32 = (unsigned int *)fbdev->vidmem[fbdev->current_frm_desc][0];
	w = mode->xres;
	h = mode->yres;
	bpp = info->var.bits_per_pixel;

	dev_info(info->dev,
			"LCD H COLOR BAR w,h,bpp(%d,%d,%d), fbdev->vidmem[0]=%p\n", w, h,
			bpp, fbdev->vidmem[fbdev->current_frm_desc][0]);

	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			short c16;
			int c32;
			switch ((i / 10) % 4) {
				case 0:
					c16 = 0xF800;
					c32 = 0x00FF0000;
					break;
				case 1:
					c16 = 0x07C0;
					c32 = 0x0000FF00;
					break;
				case 2:
					c16 = 0x001F;
					c32 = 0x000000FF;
					break;
				default:
					c16 = 0xFFFF;
					c32 = 0xFFFFFFFF;
					break;
			}
			switch (bpp) {
				case 18:
				case 24:
				case 32:
					*p32++ = c32;
					break;
				default:
					*p16++ = c16;
			}
		}
		if (w % PIXEL_ALIGN) {
			switch (bpp) {
				case 18:
				case 24:
				case 32:
					p32 += (ALIGN(mode->xres, PIXEL_ALIGN) - w);
					break;
				default:
					p16 += (ALIGN(mode->xres, PIXEL_ALIGN) - w);
					break;
			}
		}
	}
}

int lcd_display_inited_by_uboot( void )
{
	if (*(unsigned int*)(0xb3050000 + DC_ST) & DC_WORKING)
		uboot_inited = 1;
	else
		uboot_inited = 0;
	return uboot_inited;
}

static int slcd_pixel_refresh_times(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;
	struct lcd_panel *panel = fbdev->panel;
	struct smart_config *smart_config = panel->smart_config;

	switch(smart_config->smart_type){
	case SMART_LCD_TYPE_8080:
	case SMART_LCD_TYPE_6800:
		break;
	case SMART_LCD_TYPE_SPI_3:
		return 9;
	case SMART_LCD_TYPE_SPI_4:
		return 8;
	default:
		printk("%s %d err!\n",__func__,__LINE__);
		break;
	}

	switch(smart_config->pix_fmt) {
	case SMART_LCD_FORMAT_888:
		return 3;
	case SMART_LCD_FORMAT_565:
		return 2;
	default:
		printk("%s %d err!\n",__func__,__LINE__);
		break;
	}

	return 1;
}

static int refresh_pixclock_auto_adapt(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;
	struct lcd_panel *panel = fbdev->panel;
	struct fb_var_screeninfo *var = &info->var;
	struct fb_videomode *mode;
	uint16_t hds, vds;
	uint16_t hde, vde;
	uint16_t ht, vt;
	unsigned long rate;

	mode = panel->modes;
	if (mode == NULL) {
		dev_err(fbdev->dev, "%s get video mode failed\n", __func__);
		return -EINVAL;
	}

	hds = mode->hsync_len + mode->left_margin;
	hde = hds + mode->xres;
	ht = hde + mode->right_margin;

	vds = mode->vsync_len + mode->upper_margin;
	vde = vds + mode->yres;
	vt = vde + mode->lower_margin;

	if(mode->refresh){
		rate = mode->refresh * vt * ht;
		if(fbdev->panel->lcd_type == LCD_TYPE_SLCD) {
			/* pixclock frequency is wr 2.5 times */
			rate = rate * 5 / 2;
			rate *= slcd_pixel_refresh_times(info);
		}
		mode->pixclock = KHZ2PICOS(rate / 1000);

		var->pixclock = mode->pixclock;
	}else if(mode->pixclock){
		rate = PICOS2KHZ(mode->pixclock) * 1000;
		mode->refresh = rate / vt / ht;
	}else{
		dev_err(fbdev->dev,"%s error:lcd important config info is absenced\n",__func__);
		return -EINVAL;
	}

	return 0;
}

static void ingenicfb_enable(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;
	struct lcd_panel *panel = fbdev->panel;

	mutex_lock(&fbdev->lock);
	if (fbdev->is_lcd_en) {
		mutex_unlock(&fbdev->lock);
		return;
	}

	ingenicfb_cmp_start(info);

	if(panel->lcd_type == LCD_TYPE_SLCD) {
		slcd_send_mcu_command(fbdev, panel->smart_config->write_gram_cmd);
		wait_slcd_busy();
		ingenicfb_slcd_start(info);
	} else {
		ingenicfb_tft_start(info);
	}

	fbdev->is_lcd_en = 1;
	mutex_unlock(&fbdev->lock);
	return;
}

static void ingenicfb_disable(struct fb_info *info, stop_mode_t stop_md)
{
	mutex_lock(&fbdev->lock);
	if (!fbdev->is_lcd_en) {
		mutex_unlock(&fbdev->lock);
		return;
	}

	if(stop_md == QCK_STOP) {
		reg_write(fbdev, DC_CTRL, DC_QCK_STP_CMP);
		wait_dc_state(DC_WORKING, 0);
	} else {
		reg_write(fbdev, DC_CTRL, DC_GEN_STP_CMP);
		wait_event_interruptible(fbdev->gen_stop_wq,
				!(reg_read(fbdev, DC_ST) & DC_WORKING));
	}

	fbdev->is_lcd_en = 0;
	mutex_unlock(&fbdev->lock);
}

static int ingenicfb_desc_init(struct fb_info *info, int frm_num)
{
	struct ingenicfb_device *fbdev = info->par;
	struct fb_videomode *mode;
	struct ingenicfb_frm_mode *frm_mode;
	struct ingenicfb_frm_cfg *frm_cfg;
	struct ingenicfb_lay_cfg *lay_cfg;
	struct fb_var_screeninfo *var = &info->var;
	struct ingenicfb_framedesc **framedesc;
	int frm_num_mi, frm_num_ma;
	int frm_size;
	int i, j;
	int ret = 0;

	mode = ingenicfb_get_mode(var, info);
	if (mode == NULL) {
		dev_err(info->dev, "%s get video mode failed\n", __func__);
		return -EINVAL;
	}

	framedesc = fbdev->framedesc;
	frm_mode = &fbdev->current_frm_mode;
	frm_cfg = &frm_mode->frm_cfg;
	lay_cfg = frm_cfg->lay_cfg;

	ret = ingenicfb_check_frm_cfg(info, frm_cfg);
	if(ret) {
		dev_err(info->dev, "%s configure framedesc[%d] error!\n", __func__, frm_num);
		return ret;
	}

	if(frm_num == FRAME_CFG_ALL_UPDATE) {
		frm_num_mi = 0;
		frm_num_ma = MAX_DESC_NUM;
	} else {
		if(frm_num < 0 || frm_num > MAX_DESC_NUM) {
			dev_err(info->dev, "framedesc num err!\n");
			return -EINVAL;
		}
		frm_num_mi = frm_num;
		frm_num_ma = frm_num + 1;
	}

	for(i = frm_num_mi; i < frm_num_ma; i++) {
		framedesc[i]->FrameNextCfgAddr = fbdev->framedesc_phys[i];
		framedesc[i]->FrameSize.b.width = mode->xres;
		framedesc[i]->FrameSize.b.height = mode->yres;
		framedesc[i]->FrameCtrl.d32 = FRAME_CTRL_DEFAULT_SET;
		framedesc[i]->Layer0CfgAddr = fbdev->layerdesc_phys[i][0];
		framedesc[i]->Layer1CfgAddr = fbdev->layerdesc_phys[i][1];
		framedesc[i]->LayCfgEn.b.lay0_en = lay_cfg[0].lay_en;
		framedesc[i]->LayCfgEn.b.lay1_en = lay_cfg[1].lay_en;
		framedesc[i]->LayCfgEn.b.lay0_z_order = lay_cfg[0].lay_z_order;
		framedesc[i]->LayCfgEn.b.lay1_z_order = lay_cfg[1].lay_z_order;
		if(fbdev->panel->lcd_type == LCD_TYPE_SLCD && !fbdev->slcd_continua) {
			framedesc[i]->FrameCtrl.b.stop = 1;
			framedesc[i]->InterruptControl.d32 = DC_EOD_MSK;
		} else {
			framedesc[i]->FrameCtrl.b.stop = 0;
			framedesc[i]->InterruptControl.d32 = DC_SOF_MSK;
		}
	}

	frm_size = mode->xres * mode->yres;
	fbdev->frm_size = frm_size * info->var.bits_per_pixel >> 3;
	info->screen_size = fbdev->frm_size;

	for(i = frm_num_mi; i < frm_num_ma; i++) {
		for(j =0; j < MAX_LAYER_NUM; j++) {
			if(!lay_cfg[j].lay_en)
				continue;
			fbdev->layerdesc[i][j]->LayerSize.b.width = lay_cfg[j].pic_width;
			fbdev->layerdesc[i][j]->LayerSize.b.height = lay_cfg[j].pic_height;
			fbdev->layerdesc[i][j]->LayerPos.b.x_pos = lay_cfg[j].disp_pos_x;
			fbdev->layerdesc[i][j]->LayerPos.b.y_pos = lay_cfg[j].disp_pos_y;
			fbdev->layerdesc[i][j]->LayerCfg.b.g_alpha_en = lay_cfg[j].g_alpha_en;
			fbdev->layerdesc[i][j]->LayerCfg.b.g_alpha = lay_cfg[j].g_alpha_val;
			fbdev->layerdesc[i][j]->LayerCfg.b.color = lay_cfg[j].color;
			fbdev->layerdesc[i][j]->LayerCfg.b.domain_multi = lay_cfg[j].domain_multi;
			fbdev->layerdesc[i][j]->LayerCfg.b.format = lay_cfg[j].format;
			fbdev->layerdesc[i][j]->LayerStride = lay_cfg[j].stride;
			fbdev->vidmem_phys[i][j] = fbdev->vidmem_phys[0][0] +
						   i * fbdev->frm_size +
						   j * fbdev->frm_size * MAX_DESC_NUM;
			fbdev->vidmem[i][j] = fbdev->vidmem[0][0] +
						   i * fbdev->frm_size +
						   j * fbdev->frm_size * MAX_DESC_NUM;
			fbdev->layerdesc[i][j]->LayerBufferAddr = fbdev->vidmem_phys[i][j];
		}
	}

	for(i = frm_num_mi; i < frm_num_ma; i++) {
		frm_mode->update_st[i] = FRAME_CFG_UPDATE;
	}

	return 0;
}

static int ingenicfb_update_frm_mode(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;
	struct fb_videomode *mode;
	struct ingenicfb_frm_mode *frm_mode;
	struct ingenicfb_frm_cfg *frm_cfg;
	struct ingenicfb_lay_cfg *lay_cfg;
	struct fb_var_screeninfo *var = &info->var;
	int i;

	mode = ingenicfb_get_mode(var, info);
	if (mode == NULL) {
		dev_err(info->dev, "%s get video mode failed\n", __func__);
		return -EINVAL;
	}

	frm_mode = &fbdev->current_frm_mode;
	frm_cfg = &frm_mode->frm_cfg;
	lay_cfg = frm_cfg->lay_cfg;

	/*Only set layer0 work*/
	lay_cfg[0].lay_en = 1;
	lay_cfg[0].lay_z_order = 1;
	lay_cfg[1].lay_en = 0;
	lay_cfg[1].lay_z_order = 0;

	for(i = 0; i < MAX_LAYER_NUM; i++) {
		lay_cfg[i].pic_width = mode->xres;
		lay_cfg[i].pic_height = mode->yres;
		lay_cfg[i].disp_pos_x = 0;
		lay_cfg[i].disp_pos_y = 0;
		lay_cfg[i].g_alpha_en = 0;
		lay_cfg[i].g_alpha_val = 0xff;
		lay_cfg[i].color = ingenicfb_colormodes[0].color;
		lay_cfg[i].format = ingenicfb_colormodes[0].mode;
		lay_cfg[i].domain_multi = 1;
		lay_cfg[i].stride = mode->xres;
	}

	fbdev->current_frm_desc = 0;

	return 0;
}

static void disp_common_init(struct lcd_panel *ingenicfb_panel) {
	uint32_t disp_com;

	disp_com = reg_read(fbdev, DC_DISP_COM);
	disp_com &= ~DC_DP_IF_SEL;
	if(ingenicfb_panel->lcd_type == LCD_TYPE_SLCD) {
		disp_com |= DC_DISP_COM_SLCD;
	} else {
		disp_com |= DC_DISP_COM_TFT;
	}
	if(ingenicfb_panel->dither_enable) {
		disp_com |= DC_DP_DITHER_EN;
		disp_com &= ~DC_DP_DITHER_DW_MASK;
		disp_com |= ingenicfb_panel->dither.dither_red
			     << DC_DP_DITHER_DW_RED_LBIT;
		disp_com |= ingenicfb_panel->dither.dither_green
			    << DC_DP_DITHER_DW_GREEN_LBIT;
		disp_com |= ingenicfb_panel->dither.dither_blue
			    << DC_DP_DITHER_DW_BLUE_LBIT;
	} else {
		disp_com &= ~DC_DP_DITHER_EN;
	}
	reg_write(fbdev, DC_DISP_COM, disp_com);
}

static void common_cfg_init(void)
{
	unsigned com_cfg = 0;

	com_cfg = reg_read(fbdev, DC_COM_CONFIG);
	/*Keep COM_CONFIG reg first bit 0 */
	com_cfg &= ~DC_OUT_SEL;

	/* set burst length 32*/
	com_cfg &= ~DC_BURST_LEN_BDMA_MASK;
	com_cfg |= DC_BURST_LEN_BDMA_32;

	reg_write(fbdev, DC_COM_CONFIG, com_cfg);
}

static int ingenicfb_set_fix_par(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;
	struct lcd_panel *panel = fbdev->panel;
	unsigned int intc;
	unsigned int disp_com;
	unsigned int is_lcd_en;

	mutex_lock(&fbdev->lock);
	is_lcd_en = fbdev->is_lcd_en;
	mutex_unlock(&fbdev->lock);

	ingenicfb_disable(info, GEN_STOP);

	disp_common_init(panel);

	common_cfg_init();

	reg_write(fbdev, DC_CLR_ST, 0x01FFFFFE);

	disp_com = reg_read(fbdev, DC_DISP_COM);
	if (panel->lcd_type == LCD_TYPE_SLCD) {
		reg_write(fbdev, DC_DISP_COM, disp_com | DC_DISP_COM_SLCD);
		ingenicfb_slcd_set_par(info);
	} else {
		reg_write(fbdev, DC_DISP_COM, disp_com | DC_DISP_COM_TFT);
		ingenicfb_tft_set_par(info);
	}
	/*       disp end      gen_stop    tft_under    frm_start    frm_end  */
//	intc = DC_EOD_MSK | DC_SDA_MSK | DC_UOT_MSK | DC_SOF_MSK | DC_EOF_MSK;
	intc = DC_EOD_MSK | DC_SDA_MSK | DC_UOT_MSK | DC_SOF_MSK;
	if (panel->lcd_type == LCD_TYPE_SLCD && !fbdev->slcd_continua) {
		intc = DC_EOD_MSK | DC_SDA_MSK | DC_UOT_MSK;
	} else {
		intc = DC_SDA_MSK | DC_UOT_MSK | DC_SOF_MSK;
	}
	reg_write(fbdev, DC_INTC, intc);

	if(is_lcd_en) {
		ingenicfb_enable(info);
	}

	return 0;
}

static int ingenicfb_set_par(struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;
	struct fb_var_screeninfo *var = &info->var;
	struct fb_videomode *mode;
	struct ingenicfb_frmdesc_msg *desc_msg;
	unsigned long flags;
	uint32_t colormode;
	int ret;
	int i;

	mode = ingenicfb_get_mode(var, info);
	if (mode == NULL) {
		dev_err(info->dev, "%s get video mode failed\n", __func__);
		return -EINVAL;
	}
	info->mode = mode;

	ret = ingenicfb_check_colormode(var, &colormode);
	if(ret) {
		dev_err(info->dev,"Check colormode failed!\n");
		return  ret;
	}
	if(colormode != LAYER_CFG_FORMAT_YUV422) {
		for(i = 0; i < MAX_LAYER_NUM; i++) {
			fbdev->current_frm_mode.frm_cfg.lay_cfg[i].format = colormode;
		}
	} else {
		fbdev->current_frm_mode.frm_cfg.lay_cfg[0].format = colormode;
	}

	ret = ingenicfb_desc_init(info, FRAME_CFG_ALL_UPDATE);
	if(ret) {
		dev_err(fbdev->dev, "Desc init err!\n");
		return ret;
	}

	spin_lock_irqsave(&fbdev->irq_lock, flags);
	if(fbdev->panel->lcd_type == LCD_TYPE_SLCD && !fbdev->slcd_continua) {
		if(!list_empty(&fbdev->desc_run_list)) {
			desc_msg = list_first_entry(&fbdev->desc_run_list,
					struct ingenicfb_frmdesc_msg, list);
		} else {
			desc_msg = &fbdev->frmdesc_msg[fbdev->current_frm_desc];
			desc_msg->addr_virt->FrameCtrl.b.stop = 1;
			desc_msg->state = DESC_ST_RUNING;
			list_add_tail(&desc_msg->list, &fbdev->desc_run_list);
		}
		reg_write(fbdev, DC_FRM_CFG_ADDR, desc_msg->addr_phy);
	} else {
		reg_write(fbdev, DC_FRM_CFG_ADDR, fbdev->framedesc_phys[fbdev->current_frm_desc]);
	}
	spin_unlock_irqrestore(&fbdev->irq_lock, flags);

	return 0;
}

int test_pattern(struct ingenicfb_device *fbdev)
{
	int ret;

	ingenicfb_disable(fbdev->fb, QCK_STOP);
	ingenicfb_display_h_color_bar(fbdev->fb);
	fbdev->current_frm_desc = 0;
	ingenicfb_set_fix_par(fbdev->fb);
	ret = ingenicfb_set_par(fbdev->fb);
	if(ret) {
		dev_err(fbdev->dev, "Set par failed!\n");
		return ret;
	}
	ingenicfb_enable(fbdev->fb);

	return 0;
}

static inline int timeval_sub_to_us(struct timeval lhs,
						struct timeval rhs)
{
	int sec, usec;
	sec = lhs.tv_sec - rhs.tv_sec;
	usec = lhs.tv_usec - rhs.tv_usec;

	return (sec*1000000 + usec);
}

static inline int time_us2ms(int us)
{
	return (us/1000);
}

static void calculate_frame_rate(void)
{
	static struct timeval time_now, time_last;
	unsigned int interval_in_us;
	unsigned int interval_in_ms;
	static unsigned int fpsCount = 0;

	switch(showFPS){
	case 1:
		fpsCount++;
		do_gettimeofday(&time_now);
		interval_in_us = timeval_sub_to_us(time_now, time_last);
		if ( interval_in_us > (USEC_PER_SEC) ) { /* 1 second = 1000000 us. */
			printk(" Pan display FPS: %d\n",fpsCount);
			fpsCount = 0;
			time_last = time_now;
		}
		break;
	case 2:
		do_gettimeofday(&time_now);
		interval_in_us = timeval_sub_to_us(time_now, time_last);
		interval_in_ms = time_us2ms(interval_in_us);
		printk(" Pan display interval ms: %d\n",interval_in_ms);
		time_last = time_now;
		break;
	default:
		if (showFPS > 3) {
			int d, f;
			fpsCount++;
			do_gettimeofday(&time_now);
			interval_in_us = timeval_sub_to_us(time_now, time_last);
			if (interval_in_us > USEC_PER_SEC * showFPS ) { /* 1 second = 1000000 us. */
				d = fpsCount / showFPS;
				f = (fpsCount * 10) / showFPS - d * 10;
				printk(" Pan display FPS: %d.%01d\n", d, f);
				fpsCount = 0;
				time_last = time_now;
			}
		}
		break;
	}
}

static int ingenicfb_set_desc_msg(struct fb_info *info, int num)
{
	struct ingenicfb_device *fbdev = info->par;
	struct ingenicfb_frmdesc_msg *desc_msg_curr;
	struct ingenicfb_frmdesc_msg *desc_msg;
	int i;

	desc_msg_curr = &fbdev->frmdesc_msg[num];
	if((desc_msg_curr->state != DESC_ST_AVAILABLE)
			&& (desc_msg_curr->state != DESC_ST_FREE)) {
		printk(KERN_DEBUG "%s Desc state is err!\n"
			"Now there are %d bufs, hoping to use polling to improve efficiency\n"
			, __func__, MAX_DESC_NUM);
		return 0;
	}

	/* Whether the controller in the work */
	if(!(reg_read(fbdev, DC_ST) & DC_WORKING) && (list_empty(&fbdev->desc_run_list))) {
		desc_msg_curr->state = DESC_ST_RUNING;
		list_add_tail(&desc_msg_curr->list, &fbdev->desc_run_list);
		desc_msg_curr->addr_virt->FrameCtrl.b.stop = 1;
		reg_write(fbdev, DC_FRM_CFG_ADDR, desc_msg_curr->addr_phy);
		ingenicfb_cmp_start(info);
		ingenicfb_slcd_start(info);
	} else {
		if(list_empty(&fbdev->desc_run_list)) {
			dev_err(fbdev->dev, "desc_run_list is empty!!!!\n");
			return -EINVAL;
		}
		desc_msg = list_entry(fbdev->desc_run_list.prev,
				struct ingenicfb_frmdesc_msg, list);
		if(reg_read(fbdev, DC_FRM_DES) != desc_msg->addr_phy) {
			desc_msg->addr_virt->FrameNextCfgAddr = desc_msg_curr->addr_phy;
			desc_msg->addr_virt->FrameCtrl.b.stop = 0;
		}
		desc_msg_curr->addr_virt->FrameCtrl.b.stop = 1;
		desc_msg_curr->state = DESC_ST_RUNING;
		list_add_tail(&desc_msg_curr->list, &fbdev->desc_run_list);
	}

	for(i = 0; i < MAX_DESC_NUM; i++) {
		if(fbdev->frmdesc_msg[i].state == DESC_ST_FREE) {
			fbdev->frmdesc_msg[i].state = DESC_ST_AVAILABLE;
			fbdev->vsync_skip_map = (fbdev->vsync_skip_map >> 1 |
						fbdev->vsync_skip_map << 9) & 0x3ff;
			if(likely(fbdev->vsync_skip_map & 0x1)) {
				fbdev->timestamp.value[fbdev->timestamp.wp] =
					ktime_to_ns(ktime_get());
				fbdev->timestamp.wp = (fbdev->timestamp.wp + 1) % TIMESTAMP_CAP;
				wake_up_interruptible(&fbdev->vsync_wq);
			}
		}
	}

	return 0;
}

static int ingenicfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;
	unsigned long flags;
	int next_frm;
	int ret = 0;

	if (var->xoffset - info->var.xoffset) {
		dev_err(info->dev, "No support for X panning for now\n");
		return -EINVAL;
	}

	fbdev->pan_display_count++;
	if(showFPS){
		calculate_frame_rate();
	}

	next_frm = var->yoffset / var->yres;

//	if(fbdev->current_frm_desc == next_frm && fbdev->is_lcd_en) {
//		ingenicfb_disable(fbdev->fb, GEN_STOP);
//	}
	if(fbdev->current_frm_mode.update_st[next_frm] == FRAME_CFG_NO_UPDATE) {
		if((ret = ingenicfb_desc_init(info, next_frm))) {
			dev_err(info->dev, "%s: desc init err!\n", __func__);
			dump_lcdc_registers();
			return ret;
		}
	}

	if(fbdev->panel->lcd_type == LCD_TYPE_SLCD && !fbdev->slcd_continua) {
		spin_lock_irqsave(&fbdev->irq_lock, flags);
		ret = ingenicfb_set_desc_msg(info, next_frm);
		spin_unlock_irqrestore(&fbdev->irq_lock, flags);
		if(ret) {
			dev_err(info->dev, "%s: Set desc_msg err!\n", __func__);
			return ret;
		}
	} else {
		reg_write(fbdev, DC_FRM_CFG_ADDR, fbdev->framedesc_phys[next_frm]);
	}

	if(!fbdev->is_lcd_en)
		ingenicfb_enable(info);
	fbdev->current_frm_desc = next_frm;

	return 0;
}

static void ingenicfb_do_resume(struct ingenicfb_device *fbdev)
{
	int ret;

	mutex_lock(&fbdev->suspend_lock);
	if(fbdev->is_suspend) {
		ingenicfb_clk_enable(fbdev);
		ingenicfb_set_fix_par(fbdev->fb);
		ret = ingenicfb_set_par(fbdev->fb);
		if(ret) {
			dev_err(fbdev->dev, "Set par failed!\n");
		}
		ingenicfb_enable(fbdev->fb);
		fbdev->is_suspend = 0;
	}
	mutex_unlock(&fbdev->suspend_lock);
}

static void ingenicfb_do_suspend(struct ingenicfb_device *fbdev)
{
	struct lcd_panel *panel = fbdev->panel;
	struct lcd_panel_ops *panel_ops;

	panel_ops = panel->ops;

	mutex_lock(&fbdev->suspend_lock);
	if (!fbdev->is_suspend){
		ingenicfb_disable(fbdev->fb, QCK_STOP);
		ingenicfb_clk_disable(fbdev);
		if(panel_ops && panel_ops->disable)
			panel_ops->disable(panel);

		fbdev->is_suspend = 1;
	}
	mutex_unlock(&fbdev->suspend_lock);
}

static int ingenicfb_blank(int blank_mode, struct fb_info *info)
{
	struct ingenicfb_device *fbdev = info->par;

	if (blank_mode == FB_BLANK_UNBLANK) {
		ingenicfb_do_resume(fbdev);
	} else {
		ingenicfb_do_suspend(fbdev);
	}

	return 0;
}

static int ingenicfb_open(struct fb_info *info, int user)
{
	struct ingenicfb_device *fbdev = info->par;
	int ret;
	int i;

	if (!fbdev->is_lcd_en && fbdev->vidmem_phys) {
		for(i = 0; i < MAX_DESC_NUM; i++) {
			fbdev->frmdesc_msg[i].state = DESC_ST_FREE;
		}
		INIT_LIST_HEAD(&fbdev->desc_run_list);
		fbdev->timestamp.rp = 0;
		fbdev->timestamp.wp = 0;
		ingenicfb_set_fix_par(info);
		ret = ingenicfb_set_par(info);
		if(ret) {
			dev_err(info->dev, "Set par failed!\n");
			return ret;
		}
		memset(fbdev->vidmem[fbdev->current_frm_desc][0], 0, fbdev->frm_size);
		ingenicfb_enable(info);
	}

	dev_dbg(info->dev, "####open count : %d\n", ++fbdev->open_cnt);

	return 0;
}

static int ingenicfb_release(struct fb_info *info, int user)
{
	struct ingenicfb_device *fbdev = info->par;
	int i;

	dev_dbg(info->dev, "####close count : %d\n", fbdev->open_cnt--);
	if(!fbdev->open_cnt) {
		for(i = 0; i < MAX_DESC_NUM; i++) {
			fbdev->frmdesc_msg[i].state = DESC_ST_FREE;
		}
		INIT_LIST_HEAD(&fbdev->desc_run_list);
		fbdev->timestamp.rp = 0;
		fbdev->timestamp.wp = 0;
	}
	return 0;
}

static ssize_t ingenicfb_write(struct fb_info *info, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct ingenicfb_device *fbdev = info->par;
	u8 *buffer, *src;
	u8 __iomem *dst;
	int c, cnt = 0, err = 0;
	unsigned long total_size;
	unsigned long p = *ppos;
	unsigned long flags;
	int next_frm = 0;
	int ret;

	total_size = info->screen_size;

	if (total_size == 0)
		total_size = info->fix.smem_len;

	if (p > total_size)
		return -EFBIG;

	if (count > total_size) {
		err = -EFBIG;
		count = total_size;
	}

	if (count + p > total_size) {
		if (!err)
			err = -ENOSPC;

		count = total_size - p;
	}

	buffer = kmalloc((count > PAGE_SIZE) ? PAGE_SIZE : count,
			 GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	dst = (u8 __iomem *) (info->screen_base + p);

	while (count) {
		c = (count > PAGE_SIZE) ? PAGE_SIZE : count;
		src = buffer;

		if (copy_from_user(src, buf, c)) {
			err = -EFAULT;
			break;
		}

		fb_memcpy_tofb(dst, src, c);
		dst += c;
		src += c;
		*ppos += c;
		buf += c;
		cnt += c;
		count -= c;
	}

	kfree(buffer);

	if(fbdev->panel->lcd_type == LCD_TYPE_SLCD && !fbdev->slcd_continua) {
		spin_lock_irqsave(&fbdev->irq_lock, flags);
		ret = ingenicfb_set_desc_msg(info, next_frm);
		spin_unlock_irqrestore(&fbdev->irq_lock, flags);
		if(ret) {
			dev_err(info->dev, "%s: Set desc_msg err!\n", __func__);
			return ret;
		}
	} else {
		reg_write(fbdev, DC_FRM_CFG_ADDR, fbdev->framedesc_phys[next_frm]);
	}

	if(!fbdev->is_lcd_en)
		ingenicfb_enable(info);
	fbdev->current_frm_desc = next_frm;

	return (cnt) ? cnt : err;
}

static struct fb_ops ingenicfb_ops = {
	.owner = THIS_MODULE,
	.fb_open = ingenicfb_open,
	.fb_release = ingenicfb_release,
	.fb_write = ingenicfb_write,
	.fb_check_var = ingenicfb_check_var,
	.fb_set_par = ingenicfb_set_par,
	.fb_setcolreg = ingenicfb_setcolreg,
	.fb_blank = ingenicfb_blank,
	.fb_pan_display = ingenicfb_pan_display,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_ioctl = ingenicfb_ioctl,
	.fb_mmap = ingenicfb_mmap,
};

	static ssize_t
dump_h_color_bar(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ingenicfb_device *fbdev = dev_get_drvdata(dev);
	ingenicfb_display_h_color_bar(fbdev->fb);
	if (fbdev->panel->lcd_type == LCD_TYPE_SLCD && !fbdev->slcd_continua) {
		ingenicfb_cmp_start(fbdev->fb);
		ingenicfb_slcd_start(fbdev->fb);
	}
	return 0;
}

	static ssize_t
dump_v_color_bar(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ingenicfb_device *fbdev = dev_get_drvdata(dev);
	ingenicfb_display_v_color_bar(fbdev->fb);
	if (fbdev->panel->lcd_type == LCD_TYPE_SLCD && !fbdev->slcd_continua) {
		ingenicfb_cmp_start(fbdev->fb);
		ingenicfb_slcd_start(fbdev->fb);
	}
	return 0;
}

	static ssize_t
vsync_skip_r(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ingenicfb_device *fbdev = dev_get_drvdata(dev);
	mutex_lock(&fbdev->lock);
	snprintf(buf, 3, "%d\n", fbdev->vsync_skip_ratio);
	printk("vsync_skip_map = 0x%08x\n", fbdev->vsync_skip_map);
	mutex_unlock(&fbdev->lock);
	return 3;		/* sizeof ("%d\n") */
}

static int vsync_skip_set(struct ingenicfb_device *fbdev, int vsync_skip)
{
	unsigned int map_wide10 = 0;
	int rate, i, p, n;
	int fake_float_1k;

	if (vsync_skip < 0 || vsync_skip > 9)
		return -EINVAL;

	rate = vsync_skip + 1;
	fake_float_1k = 10000 / rate;	/* 10.0 / rate */

	p = 1;
	n = (fake_float_1k * p + 500) / 1000;	/* +0.5 to int */

	for (i = 1; i <= 10; i++) {
		map_wide10 = map_wide10 << 1;
		if (i == n) {
			map_wide10++;
			p++;
			n = (fake_float_1k * p + 500) / 1000;
		}
	}
	mutex_lock(&fbdev->lock);
	fbdev->vsync_skip_map = map_wide10;
	fbdev->vsync_skip_ratio = rate - 1;	/* 0 ~ 9 */
	mutex_unlock(&fbdev->lock);

	printk("vsync_skip_ratio = %d\n", fbdev->vsync_skip_ratio);
	printk("vsync_skip_map = 0x%08x\n", fbdev->vsync_skip_map);

	return 0;
}

	static ssize_t
vsync_skip_w(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ingenicfb_device *fbdev = dev_get_drvdata(dev);

	if ((count != 1) && (count != 2))
		return -EINVAL;
	if ((*buf < '0') && (*buf > '9'))
		return -EINVAL;

	vsync_skip_set(fbdev, *buf - '0');

	return count;
}

static ssize_t fps_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	printk("\n-----you can choice print way:\n");
	printk("Example: echo NUM > show_fps\n");
	printk("NUM = 0: close fps statistics\n");
	printk("NUM = 1: print recently fps\n");
	printk("NUM = 2: print interval between last and this pan_display\n");
	printk("NUM = 3: print pan_display count\n\n");
	return 0;
}

static ssize_t fps_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	int num = 0;
	num = simple_strtoul(buf, NULL, 0);
	if(num < 0){
		printk("\n--please 'cat show_fps' to view using the method\n\n");
		return n;
	}
	showFPS = num;
	if(showFPS == 3)
		printk(KERN_DEBUG " Pand display count=%d\n",fbdev->pan_display_count);
	return n;
}


	static ssize_t
debug_clr_st(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ingenicfb_device *fbdev = dev_get_drvdata(dev);
	reg_write(fbdev, DC_CLR_ST, 0xffffffff);
	return 0;
}

static ssize_t test_suspend(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	int num = 0;
	num = simple_strtoul(buf, NULL, 0);
	printk("0 --> resume | 1 --> suspend\nnow input %d\n", num);
	if (num == 0) {
		ingenicfb_do_resume(fbdev);
	} else {
		ingenicfb_do_suspend(fbdev);
	}
	return n;
}

/**********************lcd_debug***************************/
static DEVICE_ATTR(dump_lcd, S_IRUGO|S_IWUSR, dump_lcd, NULL);
static DEVICE_ATTR(dump_h_color_bar, S_IRUGO|S_IWUSR, dump_h_color_bar, NULL);
static DEVICE_ATTR(dump_v_color_bar, S_IRUGO|S_IWUSR, dump_v_color_bar, NULL);
static DEVICE_ATTR(vsync_skip, S_IRUGO|S_IWUSR, vsync_skip_r, vsync_skip_w);
static DEVICE_ATTR(show_fps, S_IRUGO|S_IWUSR, fps_show, fps_store);
static DEVICE_ATTR(debug_clr_st, S_IRUGO|S_IWUSR, debug_clr_st, NULL);
static DEVICE_ATTR(test_suspend, S_IRUGO|S_IWUSR, NULL, test_suspend);

static struct attribute *lcd_debug_attrs[] = {
	&dev_attr_dump_lcd.attr,
	&dev_attr_dump_h_color_bar.attr,
	&dev_attr_dump_v_color_bar.attr,
	&dev_attr_vsync_skip.attr,
	&dev_attr_show_fps.attr,
	&dev_attr_debug_clr_st.attr,
	&dev_attr_test_suspend.attr,
	NULL,
};

const char lcd_group_name[] = "debug";
static struct attribute_group lcd_debug_attr_group = {
	.name	= lcd_group_name,
	.attrs	= lcd_debug_attrs,
};

static void ingenicfb_free_devmem(struct ingenicfb_device *fbdev)
{
	size_t buff_size;

	dma_free_coherent(fbdev->dev,
			  fbdev->vidmem_size,
			  fbdev->vidmem[0][0],
			  fbdev->vidmem_phys[0][0]);

	buff_size = sizeof(struct ingenicfb_layerdesc);
	buff_size = ALIGN(buff_size, DESC_ALIGN);
	dma_free_coherent(fbdev->dev,
			  buff_size * MAX_DESC_NUM * MAX_LAYER_NUM,
			  fbdev->layerdesc[0],
			  fbdev->layerdesc_phys[0][0]);

	buff_size = sizeof(struct ingenicfb_framedesc);
	buff_size = ALIGN(buff_size, DESC_ALIGN);
	dma_free_coherent(fbdev->dev,
			  buff_size * MAX_DESC_NUM,
			  fbdev->framedesc[0],
			  fbdev->framedesc_phys[0]);
}

static int ingenicfb_copy_logo(struct fb_info *info)
{
	unsigned long src_addr = 0;	/* u-boot logo buffer address */
	unsigned long dst_addr = 0;	/* kernel frame buffer address */
	struct ingenicfb_device *fbdev = info->par;
	unsigned long size;
	unsigned int ctrl;
	unsigned read_times;
	lay_cfg_en_t lay_cfg_en;

	/* Sure the uboot SLCD using the continuous mode, Close irq */
	if (!(reg_read(fbdev, DC_ST) & DC_WORKING)) {
		dev_err(fbdev->dev, "uboot is not display logo!\n");
		return -ENOEXEC;
	}

	/*fbdev->is_lcd_en = 1;*/

	/* Reading Desc from regisger need reset */
	ctrl = reg_read(fbdev, DC_CTRL);
	ctrl |= DC_DES_CNT_RST;
	reg_write(fbdev, DC_CTRL, ctrl);

	/* For geting LayCfgEn need read  DC_FRM_DES 10 times */
	read_times = 10 - 1;
	while(read_times--) {
		reg_read(fbdev, DC_FRM_DES);
	}
	lay_cfg_en.d32 = reg_read(fbdev, DC_FRM_DES);
	if(!lay_cfg_en.b.lay0_en) {
		dev_err(fbdev->dev, "Uboot initialization is not using layer0!\n");
		return -ENOEXEC;
	}

	/* For geting LayerBufferAddr need read  DC_LAY0_DES 3 times */
	read_times = 3 - 1;
	/* get buffer physical address */
	while(read_times--) {
		reg_read(fbdev, DC_LAY0_DES);
	}
	src_addr = (unsigned long)reg_read(fbdev, DC_LAY0_DES);

	if (src_addr) {
		size = info->fix.line_length * info->var.yres;
		src_addr = (unsigned long)phys_to_virt(src_addr);
		dst_addr = (unsigned long)fbdev->vidmem[0][0];
		memcpy((void *)dst_addr, (void *)src_addr, size);
	}

	return 0;
}

static int ingenicfb_do_probe(struct platform_device *pdev, struct lcd_panel *panel)
{
	struct fb_videomode *video_mode;
	struct fb_info *fb;
	unsigned long rate;
	int ret = 0;
	int i;

	fb = framebuffer_alloc(sizeof(struct ingenicfb_device), &pdev->dev);
	if (!fb) {
		dev_err(&pdev->dev, "Failed to allocate framebuffer device\n");
		return -ENOMEM;
	}

	fbdev = fb->par;
	fbdev->fb = fb;
	fbdev->dev = &pdev->dev;
	fbdev->panel = panel;
#ifdef CONFIG_SLCDC_CONTINUA
	if(fbdev->panel->lcd_type == LCD_TYPE_SLCD)
		fbdev->slcd_continua = 1;
#endif

	spin_lock_init(&fbdev->irq_lock);
	mutex_init(&fbdev->lock);
	mutex_init(&fbdev->suspend_lock);
	sprintf(fbdev->clk_name, "gate_lcd");
	sprintf(fbdev->pclk_name, "cgu_lcd");

	fbdev->clk = devm_clk_get(&pdev->dev, fbdev->clk_name);
	fbdev->pclk = devm_clk_get(&pdev->dev, fbdev->pclk_name);

	if (IS_ERR(fbdev->clk) || IS_ERR(fbdev->pclk)) {
		ret = PTR_ERR(fbdev->clk);
		dev_err(&pdev->dev, "Failed to get lcdc clock: %d\n", ret);
		goto err_framebuffer_release;
	}

	fbdev->base = of_iomap(pdev->dev.of_node, 0);
	if (!fbdev->base) {
		dev_err(&pdev->dev,
				"Failed to ioremap register memory region\n");
		ret = -EBUSY;
		goto err_put_clk;
	}

	ret = refresh_pixclock_auto_adapt(fb);
	if(ret){
		goto err_iounmap;
	}

	video_mode = fbdev->panel->modes;
	if (!video_mode) {
		ret = -ENXIO;
		goto err_iounmap;
	}

	fb_videomode_to_modelist(panel->modes, panel->num_modes, &fb->modelist);

	ingenicfb_videomode_to_var(&fb->var, video_mode, fbdev->panel->lcd_type);
	fb->fbops = &ingenicfb_ops;
	fb->flags = FBINFO_DEFAULT;
	fb->var.width = panel->width;
	fb->var.height = panel->height;

	ingenicfb_colormode_to_var(&fb->var, &ingenicfb_colormodes[0]);

	ret = ingenicfb_check_var(&fb->var, fb);
	if (ret) {
		goto err_iounmap;
	}

	rate = PICOS2KHZ(fb->var.pixclock) * 1000;
	clk_set_rate(fbdev->pclk, rate);

	if(rate != clk_get_rate(fbdev->pclk)) {
		dev_err(&pdev->dev, "expect rate = %ld, actual rate = %ld\n", rate, clk_get_rate(fbdev->pclk));

	}

	clk_prepare_enable(fbdev->clk);
	clk_prepare_enable(fbdev->pclk);
	fbdev->is_clk_en = 1;

	ret = ingenicfb_alloc_devmem(fbdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to allocate video memory\n");
		goto err_iounmap;
	}

	fb->fix = ingenicfb_fix;
	fb->fix.line_length = (fb->var.bits_per_pixel * fb->var.xres) >> 3;
	fb->fix.smem_start = fbdev->vidmem_phys[0][0];
	fb->fix.smem_len = fbdev->vidmem_size;
	fb->screen_size = fbdev->frm_size;
	fb->screen_base = fbdev->vidmem[0][0];
	fb->pseudo_palette = fbdev->pseudo_palette;

	vsync_skip_set(fbdev, CONFIG_FB_VSYNC_SKIP);
	init_waitqueue_head(&fbdev->vsync_wq);
	init_waitqueue_head(&fbdev->gen_stop_wq);
	fbdev->open_cnt = 0;
	fbdev->is_lcd_en = 0;
	fbdev->timestamp.rp = 0;
	fbdev->timestamp.wp = 0;

	INIT_LIST_HEAD(&fbdev->desc_run_list);
	for(i = 0; i < MAX_DESC_NUM; i++) {
		fbdev->frmdesc_msg[i].state = DESC_ST_FREE;
		fbdev->frmdesc_msg[i].addr_virt = fbdev->framedesc[i];
		fbdev->frmdesc_msg[i].addr_phy = fbdev->framedesc_phys[i];
		fbdev->frmdesc_msg[i].index = i;
	}

	ingenicfb_update_frm_mode(fbdev->fb);

	fbdev->irq = platform_get_irq(pdev, 0);
	if(fbdev->irq < 0) {
		dev_err(fbdev->dev, "Failed to get irq resource!\n");
		goto err_free_devmem;
	}
	ret = devm_request_irq(fbdev->dev, fbdev->irq, ingenicfb_irq_handler, 0, "lcdc", fbdev);
	if(ret < 0) {
		dev_err(fbdev->dev, "Failed to request irq handler!\n");
		goto err_free_devmem;
	}

	platform_set_drvdata(pdev, fbdev);

	ret = sysfs_create_group(&fbdev->dev->kobj, &lcd_debug_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "device create sysfs group failed\n");

		ret = -EINVAL;
		goto err_free_irq;
	}

	ret = register_framebuffer(fb);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register framebuffer: %d\n",
				ret);
		goto err_free_file;
	}

	if (fbdev->vidmem_phys) {
		if (!ingenicfb_copy_logo(fbdev->fb)) {
			fbdev->is_lcd_en = 1;
			ret = ingenicfb_desc_init(fb, 0);
			if(ret) {
				dev_err(fbdev->dev, "Desc init err!\n");
				goto err_free_file;
			}
			reg_write(fbdev, DC_FRM_CFG_ADDR, fbdev->framedesc_phys[fbdev->current_frm_desc]);
		}
#ifdef CONFIG_FB_JZ_DEBUG
		test_pattern(fbdev);
#endif
	}else{

		ingenicfb_clk_disable(fbdev);
		ret = -ENOMEM;
		goto err_free_file;
	}

	return 0;

err_free_file:
	sysfs_remove_group(&fbdev->dev->kobj, &lcd_debug_attr_group);
err_free_irq:
	free_irq(fbdev->irq, fbdev);
err_free_devmem:
	ingenicfb_free_devmem(fbdev);
err_iounmap:
	iounmap(fbdev->base);
err_put_clk:

   if (fbdev->clk)
	devm_clk_put(fbdev->dev, fbdev->clk);
   if (fbdev->pclk)
	devm_clk_put(fbdev->dev, fbdev->pclk);

err_framebuffer_release:
	framebuffer_release(fb);
	return ret;
}

int ingenicfb_register_panel(struct lcd_panel *panel)
{
	WARN_ON(fbdev_panel != NULL);

	fbdev_panel = panel;
	if(fbdev_pdev != NULL) {
		return ingenicfb_do_probe(fbdev_pdev, fbdev_panel);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ingenicfb_register_panel);

static int ingenicfb_probe(struct platform_device *pdev)
{
	WARN_ON(fbdev_pdev != NULL);

	fbdev_pdev = pdev;

	if(fbdev_panel != NULL) {
		return ingenicfb_do_probe(fbdev_pdev, fbdev_panel);
	}

	return 0;
}

static int ingenicfb_remove(struct platform_device *pdev)
{
	struct ingenicfb_device *fbdev = platform_get_drvdata(pdev);

	ingenicfb_free_devmem(fbdev);
	platform_set_drvdata(pdev, NULL);

	devm_clk_put(fbdev->dev, fbdev->pclk);
	devm_clk_put(fbdev->dev, fbdev->clk);

	sysfs_remove_group(&fbdev->dev->kobj, &lcd_debug_attr_group);

	iounmap(fbdev->base);

	framebuffer_release(fbdev->fb);

	return 0;
}

static void ingenicfb_shutdown(struct platform_device *pdev)
{
	struct ingenicfb_device *fbdev = platform_get_drvdata(pdev);
	int is_fb_blank;
	mutex_lock(&fbdev->suspend_lock);
	is_fb_blank = (fbdev->is_suspend != 1);
	fbdev->is_suspend = 1;
	mutex_unlock(&fbdev->suspend_lock);
	if (is_fb_blank)
		fb_blank(fbdev->fb, FB_BLANK_POWERDOWN);
};

#ifdef CONFIG_PM

static int ingenicfb_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ingenicfb_device *fbdev = platform_get_drvdata(pdev);

	fb_blank(fbdev->fb, FB_BLANK_POWERDOWN);

	return 0;
}

static int ingenicfb_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ingenicfb_device *fbdev = platform_get_drvdata(pdev);

	fb_blank(fbdev->fb, FB_BLANK_UNBLANK);

	return 0;
}

static const struct dev_pm_ops ingenicfb_pm_ops = {
	.suspend = ingenicfb_suspend,
	.resume = ingenicfb_resume,
};
#endif

static const struct of_device_id ingenicfb_of_match[] = {
	{ .compatible = "ingenic,x2000-dpu"},
	{},
};

static struct platform_driver ingenicfb_driver = {
	.probe = ingenicfb_probe,
	.remove = ingenicfb_remove,
	.shutdown = ingenicfb_shutdown,
	.driver = {
		.name = "ingenic-fb",
		.of_match_table = ingenicfb_of_match,
#ifdef CONFIG_PM
		.pm = &ingenicfb_pm_ops,
#endif

	},
};

static int __init ingenicfb_init(void)
{
	platform_driver_register(&ingenicfb_driver);
	return 0;
}

static void __exit ingenicfb_cleanup(void)
{
	platform_driver_unregister(&ingenicfb_driver);
}


#ifdef CONFIG_EARLY_INIT_RUN
rootfs_initcall(ingenicfb_init);
#else
module_init(ingenicfb_init);
#endif

module_exit(ingenicfb_cleanup);

MODULE_DESCRIPTION("JZ LCD Controller driver");
MODULE_AUTHOR("Sean Tang <ctang@ingenic.cn>");
MODULE_LICENSE("GPL");
