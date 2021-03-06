/*
 * V4L2 Driver for Ingenic camera (CIM) host
 *
 * Copyright (C) 2014, Ingenic Semiconductor Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * This program is support Continuous physical address mapping,
 * not support sg mode now.
 */
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/slab.h>

#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/soc_camera.h>
#include <media/soc_mediabus.h>

#include <asm/dma.h>

#include "ingenic_camera.h"


//#define CIM_DUMP_DESC
//#define CIM_DUMP_REG
//#define CIM_DEBUG_FPS

#ifdef CIM_DUMP_REG
static void cim_dump_reg(struct ingenic_camera_dev *pcdev)
{
	if(pcdev == NULL) {
		printk("===>>%s,%d pcdev is NULL!\n",__func__,__LINE__);
		return;
	}
#define STRING  "\t=\t0x%08x\n"
	printk("REG_CIM_CFG" 	 STRING, readl(pcdev->base + CIM_CFG));
	printk("REG_CIM_CTRL" 	 STRING, readl(pcdev->base + CIM_CTRL));
	printk("REG_CIM_CTRL2" 	 STRING, readl(pcdev->base + CIM_CTRL2));
	printk("REG_CIM_STATE" 	 STRING, readl(pcdev->base + CIM_STATE));

	printk("REG_CIM_IMR" 	 STRING, readl(pcdev->base + CIM_IMR));
	printk("REG_CIM_IID" 	 STRING, readl(pcdev->base + CIM_IID));
	printk("REG_CIM_DA" 	 STRING, readl(pcdev->base + CIM_DA));
	printk("REG_CIM_FA" 	 STRING, readl(pcdev->base + CIM_FA));

	printk("REG_CIM_FID" 	 STRING, readl(pcdev->base + CIM_FID));
	printk("REG_CIM_CMD" 	 STRING, readl(pcdev->base + CIM_CMD));
	printk("REG_CIM_WSIZE" 	 STRING, readl(pcdev->base + CIM_SIZE));
	printk("REG_CIM_WOFFSET" STRING, readl(pcdev->base + CIM_OFFSET));

	printk("REG_CIM_FS" 	 STRING, readl(pcdev->base + CIM_FS));
}
#endif

static int ingenic_camera_querycap(struct soc_camera_host *ici, struct v4l2_capability *cap)
{
	strlcpy(cap->card, "ingenic-Camera", sizeof(cap->card));
	cap->version = VERSION_CODE;
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static unsigned int ingenic_camera_poll(struct file *file, poll_table *pt)
{
	struct soc_camera_device *icd = file->private_data;

	return vb2_poll(&icd->vb2_vidq, file, pt);
}

static int ingenic_dma_alloc_desc(struct ingenic_camera_dev *pcdev, int count)
{
	struct ingenic_camera_dma_desc *dma_desc_paddr;
	struct ingenic_camera_dma_desc *dma_desc;

	pcdev->buf_cnt = count;
	pcdev->desc_vaddr = dma_alloc_coherent(pcdev->soc_host.v4l2_dev.dev,
			sizeof(*pcdev->dma_desc) * pcdev->buf_cnt,
			(dma_addr_t *)&pcdev->dma_desc, GFP_KERNEL);

	dma_desc_paddr = (struct ingenic_camera_dma_desc *) pcdev->dma_desc;
	dma_desc = (struct ingenic_camera_dma_desc *) pcdev->desc_vaddr;

	if (!pcdev->dma_desc)
		return -ENOMEM;


#ifdef CIM_DUMP_DESC
	int i = 0;
	printk("pcdev->desc_vaddr = 0x%p, pcdev->dma_desc = 0x%p\n",pcdev->desc_vaddr, (dma_addr_t *)pcdev->dma_desc);
	for(i = 0; i < pcdev->buf_cnt; i++){
		printk("pcdev->desc_vaddr[%d] = 0x%p\n", i, &dma_desc[i]);
	}
	for(i = 0; i < pcdev->buf_cnt; i++){
		printk("dma_desc_paddr[%d] = 0x%p\n", i, &dma_desc_paddr[i]);
	}
#endif

	return 0;
}
static void ingenic_dma_free_desc(struct ingenic_camera_dev *pcdev)
{
	if(pcdev && pcdev->desc_vaddr) {
		dma_free_coherent(pcdev->soc_host.v4l2_dev.dev,
				sizeof(*pcdev->dma_desc) * pcdev->buf_cnt,
				pcdev->desc_vaddr, (dma_addr_t )pcdev->dma_desc);

		pcdev->desc_vaddr = NULL;
	}
}

static void dump_dma_desc(struct ingenic_camera_dev *pcdev)
{
#ifdef CIM_DUMP_DESC
	struct ingenic_camera_dma_desc *dma_desc;
	int nr;
	int i;
	dma_desc = (struct ingenic_camera_dma_desc *) pcdev->desc_vaddr;
	nr = pcdev->buf_cnt;

	printk("=============> dma_desc: %x\n", dma_desc);

	for(i = 0; i < nr; i++) {
		printk("dma_desc[%d].next: %08x\n", i, dma_desc[i].next);
		printk("dma_desc[%d].id: %08x\n", i, dma_desc[i].id);
		printk("dma_desc[%d].buf: %08x\n", i, dma_desc[i].buf);
		printk("dma_desc[%d].cmd: %08x\n", i, dma_desc[i].cmd);
		printk("dma_desc[%d].cb_frame: %08x\n", i, dma_desc[i].cb_frame);
		printk("dma_desc[%d].cb_len: %08x\n", i, dma_desc[i].cb_len);
		printk("dma_desc[%d].cr_frame: %08x\n", i, dma_desc[i].cr_frame);
		printk("dma_desc[%d].cr_len: %08x\n", i, dma_desc[i].cr_len);

	}
#endif
}
static void ingenic_dma_start(struct ingenic_camera_dev *pcdev)
{
	unsigned long temp = 0;
	unsigned int regval;

	/* please enable dma first, enable cim ctrl later.
	 * if enable cim ctrl first, RXFIFO can easily overflow.
	 */
	regval = (unsigned int)(pcdev->dma_desc);
	writel(regval, pcdev->base + CIM_DA);

	writel(0, pcdev->base + CIM_STATE);

	/*enable dma*/
	temp = readl(pcdev->base + CIM_CTRL);
	temp |= CIM_CTRL_DMA_EN;
	writel(temp, pcdev->base + CIM_CTRL);

	/* clear rx fifo */
	temp = readl(pcdev->base + CIM_CTRL);
	temp |= CIM_CTRL_RXF_RST;
	writel(temp, pcdev->base + CIM_CTRL);

	temp = readl(pcdev->base + CIM_CTRL);
	temp &= ~(CIM_CTRL_RXF_RST);
	writel(temp, pcdev->base + CIM_CTRL);

	/* enable cim */
	temp = readl(pcdev->base + CIM_CTRL);
	temp |= CIM_CTRL_ENA;
	writel(temp, pcdev->base + CIM_CTRL);
}

static void ingenic_dma_stop(struct ingenic_camera_dev *pcdev)
{
	unsigned long temp = 0;

	/* unmask all interrupts. */
	writel(0xffffffff, pcdev->base + CIM_IMR);

	/* clear rx fifo */
	temp = readl(pcdev->base + CIM_CTRL);
	temp |= CIM_CTRL_RXF_RST | CIM_CTRL_CIM_RST;
	writel(temp, pcdev->base + CIM_CTRL);

	writel(0, pcdev->base + CIM_STATE);

	/* disable dma & cim */
	temp = readl(pcdev->base + CIM_CTRL);
	temp &= ~(CIM_CTRL_ENA | CIM_CTRL_DMA_EN);
	writel(temp, pcdev->base + CIM_CTRL);
}


static	int ingenic_videobuf_setup(struct vb2_queue *q, const void *parg,
			   unsigned int *num_buffers, unsigned int *num_planes,
			   unsigned int sizes[], void *alloc_ctxs[])
{
	struct soc_camera_device *icd = soc_camera_from_vb2q(q);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct ingenic_camera_dev *pcdev = ici->priv;
	unsigned long size;

	size = icd->sizeimage;

	if (!*num_buffers || *num_buffers > MAX_BUFFER_NUM)
		*num_buffers = MAX_BUFFER_NUM;

	if (size * *num_buffers > MAX_VIDEO_MEM)
		*num_buffers = MAX_VIDEO_MEM / size;

	*num_planes = 1;
	sizes[0] = size;
	alloc_ctxs[0] = pcdev->alloc_ctx;


	pcdev->start_streaming_called = 0;
	pcdev->sequence = 0;

	if(ingenic_dma_alloc_desc(pcdev, *num_buffers))
		return -ENOMEM;

	dev_dbg(icd->parent, "%s, count=%d, size=%ld\n", __func__,
		*num_buffers, size);

	return 0;
}


static int ingenic_init_dma(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = soc_camera_from_vb2q(vb->vb2_queue);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct ingenic_camera_dev *pcdev = ici->priv;
	struct ingenic_camera_dma_desc *dma_desc;
	dma_addr_t dma_address;

	dma_desc = (struct ingenic_camera_dma_desc *) pcdev->desc_vaddr;

	dma_address = *(dma_addr_t *)vb2_plane_cookie(vb, 0);
	if(!dma_address) {
		dev_err(icd->parent, "Failed to setup DMA address\n");
		return -ENOMEM;
	}

	dma_desc[vb->index].id = vb->index;

	dma_desc[vb->index].buf = dma_address;

	dma_desc[vb->index].cmd = icd->sizeimage >> 2 | CIM_CMD_EOFINT | CIM_CMD_OFRCV;

	if(vb->index == 0) {
		pcdev->dma_desc_head = (struct ingenic_camera_dma_desc *)(pcdev->desc_vaddr);
	}

	if(vb->index == (pcdev->buf_cnt - 1)) {
		dma_desc[vb->index].next = (dma_addr_t) (pcdev->dma_desc);
		dma_desc[vb->index].cmd |= CIM_CMD_STOP;

		pcdev->dma_desc_tail = (struct ingenic_camera_dma_desc *)(&dma_desc[vb->index]);
	} else {
		dma_desc[vb->index].next = (dma_addr_t) (&pcdev->dma_desc[vb->index + 1]);
	}

	return 0;
}


static int ingenic_videobuf_init(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
        struct soc_camera_device *icd = soc_camera_from_vb2q(vb->vb2_queue);
        struct ingenic_buffer *buf = container_of(vbuf, struct ingenic_buffer, vb);
        int ret;

        ret = ingenic_init_dma(vb);
        if(ret) {
                dev_err(icd->parent,"%s:DMA initialization for Y/RGB failed\n", __func__);
                return ret;
        }
        INIT_LIST_HEAD(&buf->queue);

        return 0;
}

static	int ingenic_videobuf_prepare(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = soc_camera_from_vb2q(vb->vb2_queue);
	int ret = 0;

	dev_vdbg(icd->parent, "%s (vb=0x%p) 0x%p %lu\n", __func__,
		vb, vb2_plane_vaddr(vb, 0), vb2_get_plane_payload(vb, 0));

	vb2_set_plane_payload(vb, 0, icd->sizeimage);
	if (vb2_plane_vaddr(vb, 0) &&
	    vb2_get_plane_payload(vb, 0) > vb2_plane_size(vb, 0)) {
		ret = -EINVAL;
		goto out;
	}

	return 0;

out:
	return ret;
}

static	void ingenic_videobuf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct soc_camera_device *icd = soc_camera_from_vb2q(vb->vb2_queue);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);

	struct ingenic_camera_dev *pcdev = ici->priv;
	struct ingenic_buffer *buf = container_of(vbuf, struct ingenic_buffer, vb);
	unsigned long flags;
	unsigned int regval = 0;
	struct ingenic_camera_dma_desc *dma_desc_vaddr;
	struct ingenic_camera_dma_desc *dma_desc_paddr;

	dev_dbg(icd->parent, "%s (vb=0x%p) 0x%p %lu\n", __func__,
		vb, vb2_plane_vaddr(vb, 0), vb2_get_plane_payload(vb, 0));

	spin_lock_irqsave(&pcdev->lock, flags);

	list_add_tail(&buf->queue, &pcdev->capture);

	if(pcdev->active == NULL) {
		pcdev->active = buf; /* to be transfered buffer. */
	}


	if(!pcdev->start_streaming_called) {
		goto out;
	}

	/* judged vb->i */
	if((vb->index > pcdev->buf_cnt) || (vb->index < 0)) {
		dev_dbg(icd->parent, "Warning: %s, %d, vb->i > pcdev->buf_cnt || vb->i < 0, please check vb->i !!!\n",
				 __func__, vb->index);
		goto out;
	}


	dma_desc_vaddr = (struct ingenic_camera_dma_desc *) pcdev->desc_vaddr;

	if(pcdev->dma_desc_head != pcdev->dma_desc_tail) { /* Dynamic add dma desc */
		dma_desc_vaddr[vb->index].cmd |= CIM_CMD_STOP;

		pcdev->dma_desc_tail->next = (dma_addr_t) (&pcdev->dma_desc[vb->index]);
		pcdev->dma_desc_tail->cmd &= (~CIM_CMD_STOP);	/* unlink link last dma desc */

		pcdev->dma_desc_tail = (struct ingenic_camera_dma_desc *)(&dma_desc_vaddr[vb->index]); /* update newly tail */
	} else {
		if(pcdev->dma_stopped) {
		/* dma stoped could happen when all buffers are dequeued by user app.
		*  1. set CIM_CMD_STOP for current vb.
		*  2. and reconfigure CIM_DA, to start a new dma transfer.
		*  BUGs:
		*	cim will keep drop one frame when userspace failed to queue buffer in time.
		* */
			dma_desc_vaddr[vb->index].cmd |= CIM_CMD_STOP;

			pcdev->dma_desc_head = (struct ingenic_camera_dma_desc *)(&dma_desc_vaddr[vb->index]);
			pcdev->dma_desc_tail = (struct ingenic_camera_dma_desc *)(&dma_desc_vaddr[vb->index]);

			dma_desc_paddr = (struct ingenic_camera_dma_desc *) pcdev->dma_desc;

			/* Configure register CIMDA */
			regval = (unsigned int) (&dma_desc_paddr[vb->index]);
			writel(regval, pcdev->base + CIM_DA);

			pcdev->dma_stopped = 0;

		} else {

			dma_desc_vaddr[vb->index].cmd |= CIM_CMD_STOP;

			pcdev->dma_desc_tail->next = (dma_addr_t) (&pcdev->dma_desc[vb->index]);
			pcdev->dma_desc_tail->cmd &= (~CIM_CMD_STOP);

			pcdev->dma_desc_tail = (struct ingenic_camera_dma_desc *)(&dma_desc_vaddr[vb->index]);
		}
	}

out:
	spin_unlock_irqrestore(&pcdev->lock, flags);
	return;
}

static int test_platform_param(struct ingenic_camera_dev *pcdev,
			       unsigned char buswidth, unsigned long *flags)
{
	/*
	 * Platform specified synchronization and pixel clock polarities are
	 * only a recommendation and are only used during probing. The PXA270
	 * quick capture interface supports both.
	 */
	*flags = V4L2_MBUS_MASTER |
		V4L2_MBUS_HSYNC_ACTIVE_HIGH |
		V4L2_MBUS_HSYNC_ACTIVE_LOW |
		V4L2_MBUS_VSYNC_ACTIVE_HIGH |
		V4L2_MBUS_VSYNC_ACTIVE_LOW |
		V4L2_MBUS_DATA_ACTIVE_HIGH |
		V4L2_MBUS_PCLK_SAMPLE_RISING |
		V4L2_MBUS_PCLK_SAMPLE_FALLING;

	return 0;
}


static int ingenic_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct soc_camera_device *icd = soc_camera_from_vb2q(q);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct ingenic_camera_dev *pcdev = ici->priv;
	unsigned long flags = 0;

	spin_lock_irqsave(&pcdev->lock, flags);

	dump_dma_desc(pcdev);
	ingenic_dma_start(pcdev);
	pcdev->start_streaming_called = 1;
	pcdev->dma_stopped = 0;

	spin_unlock_irqrestore(&pcdev->lock, flags);


	return 0;
}

static void ingenic_stop_streaming(struct vb2_queue *q)
{
	struct soc_camera_device *icd = soc_camera_from_vb2q(q);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct ingenic_camera_dev *pcdev = ici->priv;
	struct ingenic_buffer *buf, *node;
	unsigned long flags = 0;

	spin_lock_irqsave(&pcdev->lock, flags);

	pcdev->active = NULL;

	ingenic_dma_stop(pcdev);

	list_for_each_entry_safe(buf, node, &pcdev->capture, queue) {
		list_del_init(&buf->queue);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}

	pcdev->start_streaming_called = 0;
	pcdev->dma_stopped = 1;
	spin_unlock_irqrestore(&pcdev->lock, flags);

	ingenic_dma_free_desc(pcdev);
}
static struct vb2_ops ingenic_videobuf_ops = {
	.queue_setup     = ingenic_videobuf_setup,
	.buf_init	 = ingenic_videobuf_init,
	.buf_prepare     = ingenic_videobuf_prepare,
	.buf_queue       = ingenic_videobuf_queue,
	.start_streaming = ingenic_start_streaming,
	.stop_streaming  = ingenic_stop_streaming,
	.wait_prepare    = vb2_ops_wait_prepare,
	.wait_finish     = vb2_ops_wait_finish,
};
static int ingenic_camera_init_videobuf(struct vb2_queue *q, struct soc_camera_device *icd)
{
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_USERPTR;
	q->drv_priv = icd;
	q->ops = &ingenic_videobuf_ops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->buf_struct_size = sizeof(struct ingenic_buffer);
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;

	return vb2_queue_init(q);
}

static int ingenic_camera_try_fmt(struct soc_camera_device *icd, struct v4l2_format *f)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_subdev_pad_config pad_cfg;
	struct v4l2_subdev_format format = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
	};
	struct v4l2_mbus_framefmt *mf = &format.format;
	u32 pixfmt = pix->pixelformat;
	int ret;

	dev_dbg(icd->parent, "%s: requested params: width = %d, height = %d\n",
		__func__, pix->width, pix->height);

	xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
	if (!xlate) {
		dev_warn(icd->parent, "Format %x not found\n", pixfmt);
		return -EINVAL;
	}

	/* limit to sensor capabilities */
	mf->width	= pix->width;
	mf->height	= pix->height;
	mf->field	= pix->field;
	mf->colorspace	= pix->colorspace;
	mf->code	= xlate->code;

	ret = v4l2_subdev_call(sd, pad, set_fmt, &pad_cfg, &format);
	if (ret < 0)
		return ret;

	pix->width	= mf->width;
	pix->height	= mf->height;
	pix->colorspace	= mf->colorspace;

	switch (mf->field) {
	case V4L2_FIELD_ANY:
		pix->field = V4L2_FIELD_NONE;
		break;
	case V4L2_FIELD_NONE:
		break;
	default:
		dev_err(icd->parent, "Field type %d unsupported.\n",
			mf->field);
		ret = -EINVAL;
	}

	dev_dbg(icd->parent, "%s: returned params: width = %d, height = %d\n",
		__func__, pix->width, pix->height);

	return ret;
}

static int ingenic_camera_set_fmt(struct soc_camera_device *icd, struct v4l2_format *f)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_subdev_format format = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};
	struct v4l2_mbus_framefmt *mf = &format.format;
	int ret;

	/* limit to hw supported fmts */
	dev_dbg(icd->parent, "%s: requested params: width = %d, height = %d\n",
		__func__, pix->width, pix->height);

	xlate = soc_camera_xlate_by_fourcc(icd, pix->pixelformat);
	if (!xlate) {
		dev_warn(icd->parent, "Format %x not found\n",
			 pix->pixelformat);
		return -EINVAL;
	}

	dev_dbg(icd->parent, "Plan to set format %dx%d\n",
			pix->width, pix->height);

	mf->width	= pix->width;
	mf->height	= pix->height;
	mf->field	= pix->field;
	mf->colorspace	= pix->colorspace;
	mf->code	= xlate->code;

	ret = v4l2_subdev_call(sd, pad, set_fmt, NULL, &format);
	if (ret < 0)
		return ret;

	if (mf->code != xlate->code)
		return -EINVAL;

	pix->width		= mf->width;
	pix->height		= mf->height;
	pix->field		= mf->field;
	pix->colorspace		= mf->colorspace;
	icd->current_fmt	= xlate;

	dev_dbg(icd->parent, "Finally set format %dx%d\n",
		pix->width, pix->height);

	return ret;
}

static int ingenic_camera_set_crop(struct soc_camera_device *icd, const struct v4l2_crop *a)
{
	struct v4l2_crop a_writable = *a;
	struct v4l2_rect *rect = &a_writable.c;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};
	struct v4l2_mbus_framefmt *mf = &fmt.format;
	int ret;

	soc_camera_limit_side(&rect->left, &rect->width, 0, 2, 4096);
	soc_camera_limit_side(&rect->top, &rect->height, 0, 2, 4096);

	ret = v4l2_subdev_call(sd, video, s_crop, a);
	if (ret < 0)
		return ret;

	/* The capture device might have changed its output  */
	ret = v4l2_subdev_call(sd, pad, get_fmt, NULL, &fmt);
	if (ret < 0)
		return ret;

	dev_dbg(icd->parent, "Sensor cropped %dx%d\n",
		mf->width, mf->height);

	icd->user_width		= mf->width;
	icd->user_height	= mf->height;

	return ret;
}

static int ingenic_camera_set_bus_param(struct soc_camera_device *icd)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct ingenic_camera_dev *pcdev = ici->priv;
	struct v4l2_mbus_config cfg = {.type = V4L2_MBUS_PARALLEL,};
	u32 pixfmt = icd->current_fmt->host_fmt->fourcc;
	unsigned long common_flags;
	unsigned long bus_flags = 0;
	unsigned long cfg_reg = 0;
	unsigned long ctrl_reg = 0;
	unsigned long ctrl2_reg = 0;
	unsigned long fs_reg = 0;
	unsigned long temp = 0;
	int ret;

	ret = test_platform_param(pcdev, icd->current_fmt->host_fmt->bits_per_sample,
			&bus_flags);

	ret = v4l2_subdev_call(sd, video, g_mbus_config, &cfg);
	if (!ret) {
		common_flags = soc_mbus_config_compatible(&cfg,
				bus_flags);
		if (!common_flags) {
			dev_warn(icd->parent,
					"Flags incompatible: camera 0x%x, host 0x%lx\n",
					cfg.flags, bus_flags);
			return -EINVAL;
		}
	} else if (ret != -ENOIOCTLCMD) {
		return ret;
	} else {
		common_flags = bus_flags;
	}

	/* Make choises, based on platform preferences */
	if ((common_flags & V4L2_MBUS_VSYNC_ACTIVE_HIGH) &&
	    (common_flags & V4L2_MBUS_VSYNC_ACTIVE_LOW)) {
		if (pcdev->pdata->flags & INGENIC_CAMERA_VSYNC_HIGH)
			common_flags &= ~V4L2_MBUS_VSYNC_ACTIVE_HIGH;
		else
			common_flags &= ~V4L2_MBUS_VSYNC_ACTIVE_LOW;
	}

	if ((common_flags & V4L2_MBUS_PCLK_SAMPLE_RISING) &&
	    (common_flags & V4L2_MBUS_PCLK_SAMPLE_FALLING)) {
		if (pcdev->pdata->flags & INGENIC_CAMERA_PCLK_RISING)
			common_flags &= ~V4L2_MBUS_PCLK_SAMPLE_FALLING;
		else
			common_flags &= ~V4L2_MBUS_PCLK_SAMPLE_RISING;
	}

	if ((common_flags & V4L2_MBUS_DATA_ACTIVE_HIGH) &&
	    (common_flags & V4L2_MBUS_DATA_ACTIVE_LOW)) {
		if (pcdev->pdata->flags & INGENIC_CAMERA_DATA_HIGH)
			common_flags &= ~V4L2_MBUS_DATA_ACTIVE_LOW;
		else
			common_flags &= ~V4L2_MBUS_DATA_ACTIVE_HIGH;
	}

	cfg.flags = common_flags;
	ret = v4l2_subdev_call(sd, video, s_mbus_config, &cfg);
	if (ret < 0 && ret != -ENOIOCTLCMD) {
		dev_dbg(icd->parent, "camera s_mbus_config(0x%lx) returned %d\n",
			common_flags, ret);
		return ret;
	}

	/*PCLK Polarity Set*/
	cfg_reg = (common_flags & V4L2_MBUS_PCLK_SAMPLE_FALLING) ?
		cfg_reg | CIM_CFG_PCP_HIGH : cfg_reg & (~CIM_CFG_PCP_HIGH);

	/*VSYNC Polarity Set*/
	cfg_reg = (common_flags & V4L2_MBUS_VSYNC_ACTIVE_HIGH) ?
		cfg_reg | CIM_CFG_VSP_HIGH : cfg_reg & (~CIM_CFG_VSP_HIGH);

	/*HSYNC Polarity Set*/
	cfg_reg = (common_flags & V4L2_MBUS_HSYNC_ACTIVE_LOW) ?
		cfg_reg | CIM_CFG_HSP_HIGH : cfg_reg & (~CIM_CFG_HSP_HIGH);

	cfg_reg |= CIM_CFG_DMA_BURST_INCR64 | CIM_CFG_DSM_GCM | CIM_CFG_PACK_Y0UY1V;

	ctrl_reg |= CIM_CTRL_DMA_SYNC | CIM_CTRL_FRC_1;

	ctrl2_reg |= CIM_CTRL2_APM | CIM_CTRL2_OPE |
		(1 << CIM_CTRL2_OPG_BIT) | CIM_CTRL2_FSC | CIM_CTRL2_ARIF;

	fs_reg = (icd->user_width -1) << CIM_FS_FHS_BIT | (icd->user_height -1)
		<< CIM_FS_FVS_BIT | 1 << CIM_FS_BPP_BIT;

	if(pixfmt == V4L2_PIX_FMT_SBGGR8) {
		fs_reg = (icd->user_width -1) << CIM_FS_FHS_BIT | (icd->user_height -1)
			<< CIM_FS_FVS_BIT | 0 << CIM_FS_BPP_BIT;
	}

	/*BS0 BS1 BS2 BS3 must be 00,01,02,03 when pack is b100*/

	if(cfg_reg & CIM_CFG_PACK_Y0UY1V)
		cfg_reg |= CIM_CFG_BS1_2_OBYT1 | CIM_CFG_BS2_2_OBYT2 | CIM_CFG_BS3_2_OBYT3;

	writel(cfg_reg, pcdev->base + CIM_CFG);
	writel(ctrl_reg, pcdev->base + CIM_CTRL);
	writel(ctrl2_reg, pcdev->base + CIM_CTRL2);
	writel(fs_reg, pcdev->base + CIM_FS);

#ifdef CIM_DUMP_REG
	cim_dump_reg(pcdev);
#endif

	/* enable end of frame interrupt */
	temp = readl(pcdev->base + CIM_IMR);
	temp &= ~(CIM_IMR_EOFM | CIM_IMR_STPM | CIM_IMR_STPM_1);
	writel(temp, pcdev->base + CIM_IMR);

	return 0;
}

static void ingenic_camera_clock_start(struct soc_camera_host *ici)
{
	struct ingenic_camera_dev *pcdev = ici->priv;
	clk_prepare_enable(pcdev->clk);
	clk_prepare_enable(pcdev->lcd_clk);
}

static void ingenic_camera_clock_stop(struct soc_camera_host *ici)
{
	struct ingenic_camera_dev *pcdev = ici->priv;
	clk_disable_unprepare(pcdev->clk);
	clk_disable_unprepare(pcdev->lcd_clk);
}

static int ingenic_camera_add_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct ingenic_camera_dev *pcdev = ici->priv;

	int icd_index = icd->devnum;

	if (pcdev->icd[icd_index])
		return -EBUSY;

	pcdev->icd[icd_index] = icd;

	ingenic_camera_clock_start(ici);

	dev_dbg(icd->parent, "Camera Driver attached to Camera %d\n", icd->devnum);
	return 0;
}
static void ingenic_camera_remove_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct ingenic_camera_dev *pcdev = ici->priv;

	int icd_index = icd->devnum;

	BUG_ON(icd != pcdev->icd[icd_index]);

	pcdev->icd[icd_index] = NULL;
	ingenic_camera_clock_stop(ici);

	dev_dbg(icd->parent, "Camera driver detached from camera %d\n", icd->devnum);
}


static irqreturn_t ingenic_camera_irq_handler(int irq, void *data)
{
	struct ingenic_camera_dev *pcdev = (struct ingenic_camera_dev *)data;
	struct ingenic_camera_dma_desc *dma_desc_paddr;
	unsigned long status = 0, temp = 0;
	unsigned long flags = 0;
	int index = 0, regval = 0;

	for (index = 0; index < ARRAY_SIZE(pcdev->icd); index++) {
		if (pcdev->icd[index]) {
			break;
		}
	}

	if(index == MAX_SOC_CAM_NUM) {
		return IRQ_HANDLED;
	}

	/* judged pcdev->dma_desc_head->id */
	if((pcdev->dma_desc_head->id > pcdev->buf_cnt) || (pcdev->dma_desc_head->id < 0)) {
		dev_warn(NULL, "Warning: %s, %d, pcdev->dma_desc_head->id >pcdev->buf_cnt || pcdev->dma_desc_head->id < 0, please check pcdev->dma_desc_head->id !!!\n",
				 __func__, pcdev->dma_desc_head->id);
		return IRQ_NONE;
	}

	spin_lock_irqsave(&pcdev->lock, flags);

	/* read interrupt status register */
	status = readl(pcdev->base + CIM_STATE);
	if (!status) {
		dev_warn(NULL, " cim status is NULL! \n");
		spin_unlock_irqrestore(&pcdev->lock, flags);

		return IRQ_NONE;
	}

	if(!(status & CIM_STATE_RXOF_STOP_EOF)) {
		/* other irq */
		dev_warn(NULL, "irq_handle status is 0x%lx, not judged in irq_handle\n", status);
		spin_unlock_irqrestore(&pcdev->lock, flags);
		return IRQ_HANDLED;
	}

	if(status & CIM_STATE_DMA_STOP) {
		/* clear dma interrupt status */
		temp = readl(pcdev->base + CIM_STATE);
		temp &= (~CIM_STATE_DMA_STOP);
		writel(temp, pcdev->base + CIM_STATE);

		pcdev->dma_stopped = 1;
	}

	if(status & CIM_STATE_DMA_EOF) {
		/* clear dma interrupt status */
		temp = readl(pcdev->base + CIM_STATE);
		temp &= (~CIM_STATE_DMA_EOF);
		writel(temp, pcdev->base + CIM_STATE);

		if(pcdev->active) {
			struct vb2_v4l2_buffer *vbuf = &pcdev->active->vb;
			struct ingenic_buffer *buf = container_of(vbuf, struct ingenic_buffer, vb);

			list_del_init(&buf->queue);
			v4l2_get_timestamp(&vbuf->timestamp);
			vbuf->field = pcdev->field;
			vbuf->sequence = pcdev->sequence++;
			vb2_buffer_done(&vbuf->vb2_buf, VB2_BUF_STATE_DONE);

#ifdef CIM_DEBUG_FPS
			if((vbuf->sequence % 60) == 0)  {
				pcdev->debug_ms_start = ktime_to_ms(ktime_get_real());
			} else if((vbuf->sequence % 60) == 59) {
				unsigned long long debug_ms_end = ktime_to_ms(ktime_get_real());
				unsigned int ms = debug_ms_end - pcdev->debug_ms_start;

				unsigned long long fps = 60 * 1000;
				do_div(fps, ms);

				dev_info(NULL, "===fps: %lld, start: %lld, end: %lld, sequence: %d\n",\
					fps, pcdev->debug_ms_start, debug_ms_end, pcdev->sequence);
			}
#endif
		}

		if(list_empty(&pcdev->capture)) {
			pcdev->active = NULL;
			spin_unlock_irqrestore(&pcdev->lock, flags);
			return IRQ_HANDLED;
		}

		if(pcdev->dma_desc_head != pcdev->dma_desc_tail) {
			pcdev->dma_desc_head =
				(struct ingenic_camera_dma_desc *)UNCAC_ADDR(phys_to_virt(pcdev->dma_desc_head->next));
		}

		if(pcdev->dma_stopped && !list_empty(&pcdev->capture)) {
			/* dma stop condition:
			 * 1. dma desc reach the end, and there is no more desc to be transferd.
			 *    dma need to stop.
			 * 2. if the capture list not empty, we should restart dma here.
			 *
			 * */

			dma_desc_paddr = (struct ingenic_camera_dma_desc *) pcdev->dma_desc;
			regval = (unsigned int) (&dma_desc_paddr[pcdev->dma_desc_head->id]);
			writel(regval, pcdev->base + CIM_DA);

			pcdev->dma_stopped = 0;
		}

		/* update dma active buffer.
		* Bug:
		* 	we assume that a EOF interrupt, dma only transfered one frame,
		* 	But , what if two or more frames transfered in a EOF interrupt
		* 	when system is really busy ??
		* */
		pcdev->active = list_first_entry(&pcdev->capture, struct ingenic_buffer, queue);

		spin_unlock_irqrestore(&pcdev->lock, flags);

		return IRQ_HANDLED;
	}
	spin_unlock_irqrestore(&pcdev->lock, flags);
	return IRQ_HANDLED;
}

static struct soc_camera_host_ops ingenic_soc_camera_host_ops = {
	.owner 		= THIS_MODULE,
	.add 		= ingenic_camera_add_device,
	.remove 	= ingenic_camera_remove_device,
	.set_bus_param 	= ingenic_camera_set_bus_param,
	.set_crop 	= ingenic_camera_set_crop,
	.set_fmt 	= ingenic_camera_set_fmt,
	.try_fmt 	= ingenic_camera_try_fmt,
	.init_videobuf2 = ingenic_camera_init_videobuf,
	.poll 		= ingenic_camera_poll,
	.querycap 	= ingenic_camera_querycap,
};

static int __init ingenic_camera_probe(struct platform_device *pdev) {
	int err = 0;
	unsigned int irq;
	struct resource *res;
	void __iomem *base;
	struct ingenic_camera_dev *pcdev;

	pcdev = kzalloc(sizeof(*pcdev), GFP_KERNEL);
	if (!pcdev) {
		dev_err(&pdev->dev, "Could not allocate pcdev\n");
		err = -ENOMEM;
		goto err_kzalloc;
	}

	pcdev->alloc_ctx = vb2_dma_contig_init_ctx(&pdev->dev);
	if(IS_ERR(pcdev->alloc_ctx)) {
		err = PTR_ERR(pcdev->alloc_ctx);
		goto err_alloc_ctx;
	}

	INIT_LIST_HEAD(&pcdev->capture);
	pcdev->active = NULL;

	/* resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(!res) {
		dev_err(&pdev->dev, "Could not get resource!\n");
		err = -ENODEV;
		goto err_get_resource;
	}

	/* irq */
	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(&pdev->dev, "Could not get irq!\n");
		err = -ENODEV;
		goto err_get_irq;
	}

	pcdev->clk = devm_clk_get(&pdev->dev, "gate_cim");
	if (IS_ERR(pcdev->clk)) {
		err = PTR_ERR(pcdev->clk);
		dev_err(&pdev->dev, "%s:can't get clk %s\n", __func__, "cim");
		goto err_clk_get_cim;
	}

	/*
	*  Get gate_lcd clk, for bugs:
	*  CIM module can work only with lcd clk enabled.
	* */
	pcdev->lcd_clk = devm_clk_get(&pdev->dev, "gate_lcd");
	if (IS_ERR(pcdev->lcd_clk)) {
		err = PTR_ERR(pcdev->lcd_clk);
		dev_err(&pdev->dev, "%s:can't get clk %s\n", __func__, "cim");
		goto err_clk_get_lcd;
	}

	pcdev->pdata = pdev->dev.platform_data;

	/* Request the regions. */
	if (!request_mem_region(res->start, resource_size(res), DRIVER_NAME)) {
		err = -EBUSY;
		goto err_request_mem_region;
	}
	base = ioremap(res->start, resource_size(res));
	if (!base) {
		err = -ENOMEM;
		goto err_ioremap;
	}

	spin_lock_init(&pcdev->lock);

	pcdev->res = res;
	pcdev->irq = irq;
	pcdev->base = base;

	/* request irq */
	err = devm_request_irq(&pdev->dev, pcdev->irq, ingenic_camera_irq_handler, 0,
			dev_name(&pdev->dev), pcdev);
	if(err) {
		dev_err(&pdev->dev, "request irq failed!\n");
		goto err_request_irq;
	}

	pcdev->soc_host.drv_name        = DRIVER_NAME;
	pcdev->soc_host.ops             = &ingenic_soc_camera_host_ops;
	pcdev->soc_host.priv            = pcdev;
	pcdev->soc_host.v4l2_dev.dev    = &pdev->dev;
	pcdev->soc_host.nr              = pdev->id;

	err = soc_camera_host_register(&pcdev->soc_host);
	if (err)
		goto err_soc_camera_host_register;

	dev_info(&pdev->dev, "ingenic Camera driver loaded!\n");

	return 0;

err_soc_camera_host_register:
	devm_free_irq(&pdev->dev, pcdev->irq, pcdev);
err_request_irq:
	iounmap(base);
err_ioremap:
	release_mem_region(res->start, resource_size(res));
err_request_mem_region:
	devm_clk_put(&pdev->dev, pcdev->lcd_clk);
err_clk_get_lcd:
	devm_clk_put(&pdev->dev, pcdev->clk);
err_clk_get_cim:
err_get_irq:
err_get_resource:
	vb2_dma_contig_cleanup_ctx(pcdev->alloc_ctx);
err_alloc_ctx:
	kfree(pcdev);
err_kzalloc:
	return err;

}

static int __exit ingenic_camera_remove(struct platform_device *pdev)
{
	struct soc_camera_host *soc_host = to_soc_camera_host(&pdev->dev);
	struct ingenic_camera_dev *pcdev = container_of(soc_host,
					struct ingenic_camera_dev, soc_host);
	struct resource *res;

	devm_free_irq(&pdev->dev, pcdev->irq, pcdev);

	devm_clk_put(&pdev->dev, pcdev->lcd_clk);
	devm_clk_put(&pdev->dev, pcdev->clk);

	soc_camera_host_unregister(soc_host);

	vb2_dma_contig_cleanup_ctx(pcdev->alloc_ctx);

	iounmap(pcdev->base);

	res = pcdev->res;
	release_mem_region(res->start, resource_size(res));

	kfree(pcdev);

	dev_info(&pdev->dev, "ingenic Camera driver unloaded\n");

	return 0;
}


static const struct of_device_id ingenic_camera_of_match[] = {
	{ .compatible = "ingenic,cim" },
	{ }
};
MODULE_DEVICE_TABLE(of, ingenic_camera_of_match);

static struct platform_driver ingenic_camera_driver = {
	.remove		= __exit_p(ingenic_camera_remove),
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = ingenic_camera_of_match,
	},
};

static int __init ingenic_camera_init(void) {
	/*
	 * platform_driver_probe() can save memory,
	 * but this Driver can bind to one device only.
	 */
	return platform_driver_probe(&ingenic_camera_driver, ingenic_camera_probe);
}

static void __exit ingenic_camera_exit(void) {
	return platform_driver_unregister(&ingenic_camera_driver);
}

module_init(ingenic_camera_init);
module_exit(ingenic_camera_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("qipengzhen <aric.pzqi@ingenic.com>");
MODULE_DESCRIPTION("ingenic Soc Camera Host Driver With videobuf2 interface");
MODULE_ALIAS("a ingenic-cim platform");
