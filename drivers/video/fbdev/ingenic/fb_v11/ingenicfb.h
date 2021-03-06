/* drivers/video/jz_fb_v14/jz_fb.h
 *
 * Copyright (C) 2016 Ingenic Semiconductor Inc.
 *
 * Author:clwang<chunlei.wang@ingenic.com>
 *
 * This program is free software, you can redistribute it and/or modify it
 *
 * under the terms of the GNU General Public License version 2 as published by
 *
 * the Free Software Foundation.
 */

#include <linux/fb.h>
#include "uapi_ingenicfb.h"

#ifdef CONFIG_ONE_FRAME_BUFFERS
#define MAX_DESC_NUM 1
#endif

#ifdef CONFIG_TWO_FRAME_BUFFERS
#define MAX_DESC_NUM 2
#endif

#ifdef CONFIG_THREE_FRAME_BUFFERS
#define MAX_DESC_NUM 3
#endif

#define DESC_ALIGN 8
#define MAX_DESC_NUM 3
#define MAX_BITS_PER_PIX (32)
#define MAX_STRIDE_VALUE (4096)
#define FRAME_CTRL_DEFAULT_SET  (0x04)
#define FRAME_CFG_ALL_UPDATE (0xFF)

enum ingenic_lcd_type {
	LCD_TYPE_TFT = 0,
	LCD_TYPE_SLCD =1,
};

/* smart lcd interface_type */
enum smart_lcd_type {
	SMART_LCD_TYPE_6800,
	SMART_LCD_TYPE_8080,
	SMART_LCD_TYPE_SPI_3,
	SMART_LCD_TYPE_SPI_4,
};

/* smart lcd format */
enum smart_lcd_format {
	SMART_LCD_FORMAT_565,
	SMART_LCD_FORMAT_666,
	SMART_LCD_FORMAT_888,
};

/* smart lcd command width */
enum smart_lcd_cwidth {
	SMART_LCD_CWIDTH_8_BIT,
	SMART_LCD_CWIDTH_9_BIT,
	SMART_LCD_CWIDTH_16_BIT,
	SMART_LCD_CWIDTH_18_BIT,
	SMART_LCD_CWIDTH_24_BIT,
};

/* smart lcd data width */
enum smart_lcd_dwidth {
	SMART_LCD_DWIDTH_8_BIT,
	SMART_LCD_DWIDTH_9_BIT,
	SMART_LCD_DWIDTH_16_BIT,
	SMART_LCD_DWIDTH_18_BIT,
	SMART_LCD_DWIDTH_24_BIT,
};

enum smart_config_type {
	SMART_CONFIG_DATA,
	SMART_CONFIG_PRM,
	SMART_CONFIG_CMD,
	SMART_CONFIG_UDELAY,
};

struct smart_lcd_data_table {
	enum smart_config_type type;
	unsigned int value;
};

enum tft_lcd_color_even {
	TFT_LCD_COLOR_EVEN_RGB,
	TFT_LCD_COLOR_EVEN_RBG,
	TFT_LCD_COLOR_EVEN_BGR,
	TFT_LCD_COLOR_EVEN_BRG,
	TFT_LCD_COLOR_EVEN_GBR,
	TFT_LCD_COLOR_EVEN_GRB,
};

enum tft_lcd_color_odd {
	TFT_LCD_COLOR_ODD_RGB,
	TFT_LCD_COLOR_ODD_RBG,
	TFT_LCD_COLOR_ODD_BGR,
	TFT_LCD_COLOR_ODD_BRG,
	TFT_LCD_COLOR_ODD_GBR,
	TFT_LCD_COLOR_ODD_GRB,
};

enum tft_lcd_mode {
	TFT_LCD_MODE_PARALLEL_888,
	TFT_LCD_MODE_PARALLEL_666,
	TFT_LCD_MODE_PARALLEL_565,
	TFT_LCD_MODE_SERIAL_RGB,
	TFT_LCD_MODE_SERIAL_RGBD,
};

struct tft_config {
	unsigned int pix_clk_inv:1;
	unsigned int de_dl:1;
	unsigned int sync_dl:1;
	enum tft_lcd_color_even color_even;
	enum tft_lcd_color_odd color_odd;
	enum tft_lcd_mode mode;
};

struct smart_config {
	unsigned int frm_md:1;
	unsigned int rdy_switch:1;
	unsigned int rdy_dp:1;
	unsigned int rdy_anti_jit:1;
	unsigned int te_switch:1;
	unsigned int te_md:1;
	unsigned int te_dp:1;
	unsigned int te_anti_jit:1;
	unsigned int cs_en:1;
	unsigned int cs_dp:1;
	unsigned int dc_md:1;
	unsigned int wr_md:1;
	enum smart_lcd_type smart_type;
	enum smart_lcd_format pix_fmt;
	enum smart_lcd_dwidth dwidth;
	enum smart_lcd_cwidth cwidth;
	unsigned int bus_width;

	unsigned long write_gram_cmd;
	unsigned int length_cmd;
	struct smart_lcd_data_table *data_table;
	unsigned int length_data_table;
	int (*init) (void);
	int (*gpio_for_slcd) (void);
};

struct lcd_panel_ops {
	void (*init)(void *panel);
	void (*enable)(void *panel);
	void (*disable)(void *panel);
};

struct lcd_panel {
	const char *name;
	unsigned int num_modes;
	struct fb_videomode *modes;

	enum ingenic_lcd_type lcd_type;
	unsigned int bpp;
	unsigned int width;
	unsigned int height;

	struct smart_config *smart_config;
	struct tft_config *tft_config;

	unsigned dither_enable:1;
	struct {
		unsigned dither_red;
		unsigned dither_green;
		unsigned dither_blue;
	} dither;

	struct lcd_panel_ops *ops;
};

typedef enum stop_mode {
	QCK_STOP,
	GEN_STOP,
} stop_mode_t;

typedef enum framedesc_st {
	FRAME_DESC_AVAILABLE = 1,
	FRAME_DESC_SET_OVER = 2,
	FRAME_DESC_USING = 3,
}framedesc_st_t;

typedef enum frm_cfg_st {
	FRAME_CFG_NO_UPDATE,
	FRAME_CFG_UPDATE,
}frm_cfg_st_t;

struct ingenicfb_frm_mode {
	struct ingenicfb_frm_cfg frm_cfg;
	frm_cfg_st_t update_st[MAX_DESC_NUM];
};

typedef enum {
	DESC_ST_FREE,
	DESC_ST_AVAILABLE,
	DESC_ST_RUNING,
} frm_st_t;

struct ingenicfb_frmdesc_msg {
	struct ingenicfb_framedesc *addr_virt;
	dma_addr_t addr_phy;
	struct list_head list;
	int state;
	int index;
};

struct ingenicfb_device {
	int is_lcd_en;		/* 0, disable  1, enable */
	int is_clk_en;		/* 0, disable  1, enable */
	int irq;		/* lcdc interrupt num */
	int open_cnt;
	int irq_cnt;
	int tft_undr_cnt;
	int frm_start;

	char clk_name[16];
	char pclk_name[16];
	char irq_name[16];
	struct clk *clk;
	struct clk *pclk;


	struct fb_info *fb;
	struct device *dev;
	struct lcd_panel *panel;

	void __iomem *base;
	struct resource *mem;

	size_t vidmem_size;
	void *vidmem[MAX_DESC_NUM][MAX_LAYER_NUM];
	dma_addr_t vidmem_phys[MAX_DESC_NUM][MAX_LAYER_NUM];

	int current_frm_desc;
	struct ingenicfb_frm_mode current_frm_mode;

	size_t frm_size;
	struct ingenicfb_framedesc *framedesc[MAX_DESC_NUM];
	dma_addr_t framedesc_phys[MAX_DESC_NUM];
	struct ingenicfb_layerdesc *layerdesc[MAX_DESC_NUM][MAX_LAYER_NUM];
	dma_addr_t layerdesc_phys[MAX_DESC_NUM][MAX_LAYER_NUM];

	wait_queue_head_t gen_stop_wq;
	wait_queue_head_t vsync_wq;
	unsigned int vsync_skip_map;	/* 10 bits width */
	int vsync_skip_ratio;

	struct ingenicfb_frmdesc_msg frmdesc_msg[MAX_DESC_NUM];
	struct list_head desc_run_list;

	struct ingenicfb_timestamp timestamp;

	struct mutex lock;
	struct mutex suspend_lock;
	spinlock_t	irq_lock;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	int is_suspend;
	unsigned int pan_display_count;
	int blank;
	unsigned int pseudo_palette[16];
	int slcd_continua;

#ifdef CONFIG_FB_INGENIC_MIPI_DSI
	struct dsi_device *dsi;
#endif
};

void ingenicfb_clk_enable(struct ingenicfb_device *ingenicfb);
void ingenicfb_clk_disable(struct ingenicfb_device *fbdev);
static inline unsigned long reg_read(struct ingenicfb_device *fbdev, int offset)
{
	return readl(fbdev->base + offset);
}

static inline void reg_write(struct ingenicfb_device *fbdev, int offset, unsigned long val)
{
	writel(val, fbdev->base + offset);
}

/* define in image_enh.c */
extern int ingenicfb_config_image_enh(struct fb_info *info);
extern int ingenicfb_image_enh_ioctl(struct fb_info *info, unsigned int cmd,
				unsigned long arg);
extern int update_slcd_frame_buffer(void);
extern int lcd_display_inited_by_uboot(void);
extern int ingenicfb_register_panel(struct lcd_panel *panel);
