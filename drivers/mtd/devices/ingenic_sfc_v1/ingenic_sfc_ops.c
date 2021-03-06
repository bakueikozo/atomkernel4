#include <linux/mtd/mtd.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include "spinor.h"
#include "ingenic_sfc_common.h"

int get_status(struct sfc_flash *flash, int command, int len)
{
	struct sfc_transfer transfer;
	int ret;
	static unsigned char buf[32];
	int i = 0;
	int val = 0;

	memset(buf, 0, sizeof(buf));
	memset(&transfer, 0, sizeof(transfer));
	sfc_list_init(&transfer);

	transfer.sfc_mode = TM_STD_SPI;
	transfer.cmd_info.cmd = command;

	transfer.addr_len = 0;
	transfer.addr = 0;

	transfer.cmd_info.dataen = ENABLE;
	transfer.len = len;
	transfer.data = (uint8_t *)buf;
	transfer.cur_len = 0;
	transfer.direction = GLB_TRAN_DIR_READ;

	transfer.data_dummy_bits = 0;
	transfer.ops_mode = CPU_OPS;

	ret = sfc_sync(flash->sfc, &transfer);
	if(ret) {
		dev_err(flash->dev,"sfc_sync error ! %s %s %d\n",__FILE__,__func__,__LINE__);
		ret=-EIO;
	}

	for(i = 0; i < len; i++) {
		val |= buf[i] << (i * 8);
	}

	return val;

}

/* do nothing to set quad mode, use cmd directly */
static int set_quad_mode_cmd(struct sfc_flash *flash)
{
	return 0;
}

/* write nor flash status register QE bit to set quad mode */
static int set_quad_mode_reg(struct sfc_flash *flash)
{
	unsigned int data;
	int ret;
	struct sfc_transfer transfer[2];
	struct spinor_flashinfo *nor_info = flash->flash_info;
	struct spi_nor_info *spi_nor_info = nor_info->nor_flash_info;
	struct spi_nor_cmd_info *wr_en;
	struct spi_nor_st_info *quad_set;
	struct spi_nor_st_info *quad_get;
	struct spi_nor_st_info *busy;
	uint32_t sta_reg = 0;

	wr_en = &spi_nor_info->wr_en;
	busy = &spi_nor_info->busy;
	quad_set = &spi_nor_info->quad_set;
	quad_get = &spi_nor_info->quad_get;
	data = (quad_set->val & quad_set->mask) << quad_set->bit_shift;

	memset(transfer, 0, sizeof(transfer));
	sfc_list_init(transfer);

	/* write enable */
	transfer[0].sfc_mode = wr_en->transfer_mode;
	transfer[0].cmd_info.cmd = wr_en->cmd;

	transfer[0].addr_len = wr_en->addr_nbyte;

	transfer[0].cmd_info.dataen = DISABLE;

	transfer[0].data_dummy_bits = wr_en->dummy_byte;

	/* write ops */
	transfer[1].sfc_mode = TM_STD_SPI;
	transfer[1].cmd_info.cmd = quad_set->cmd;

	transfer[1].cmd_info.dataen = ENABLE;
	transfer[1].len = quad_set->len;
	transfer[1].data = (uint8_t *)&data;
	transfer[1].direction = GLB_TRAN_DIR_WRITE;

	transfer[1].data_dummy_bits = quad_set->dummy;
	transfer[1].ops_mode = CPU_OPS;
	sfc_list_add_tail(&transfer[1], transfer);

	ret = sfc_sync(flash->sfc, transfer);
	if(ret) {
		dev_err(flash->dev,"sfc_sync error ! %s %s %d\n",__FILE__,__func__,__LINE__);
		ret=-EIO;
	}

	do {
		sta_reg = get_status(flash, busy->cmd, busy->len);
		sta_reg = (sta_reg >> busy->bit_shift) & busy->mask;
	} while (sta_reg != busy->val);

	do {
		sta_reg = get_status(flash, quad_get->cmd, quad_get->len);
		sta_reg = (sta_reg >> quad_get->bit_shift) & quad_get->mask;
	} while (sta_reg != quad_get->val);

	return ret;
}

static int write_enable(struct sfc_flash *flash)
{
	struct spinor_flashinfo *nor_info = flash->flash_info;
	struct spi_nor_info *spi_nor_info = nor_info->nor_flash_info;
	struct spi_nor_cmd_info *wr_en = &spi_nor_info->wr_en;
	struct sfc_transfer transfer;
	int ret;

	memset(&transfer, 0, sizeof(transfer));
	sfc_list_init(&transfer);

	transfer.sfc_mode = wr_en->transfer_mode;
	transfer.cmd_info.cmd = wr_en->cmd;

	transfer.addr_len = wr_en->addr_nbyte;

	transfer.cmd_info.dataen = DISABLE;

	transfer.data_dummy_bits = wr_en->dummy_byte;

	ret = sfc_sync(flash->sfc, &transfer);
	if(ret) {
		dev_err(flash->dev,"sfc_sync error ! %s %s %d\n",__FILE__,__func__,__LINE__);
		ret=-EIO;
	}
	return ret;
}

static int enter_4byte(struct sfc_flash *flash)
{
	struct spinor_flashinfo *nor_info = flash->flash_info;
	struct spi_nor_info *spi_nor_info = nor_info->nor_flash_info;
	struct spi_nor_cmd_info *en4byte = &spi_nor_info->en4byte;
	struct sfc_transfer transfer;
	int ret;

	memset(&transfer, 0, sizeof(transfer));
	sfc_list_init(&transfer);

	transfer.sfc_mode = en4byte->transfer_mode;
	transfer.cmd_info.cmd = en4byte->cmd;

	transfer.addr_len = en4byte->addr_nbyte;

	transfer.cmd_info.dataen = DISABLE;

	transfer.data_dummy_bits = en4byte->dummy_byte;

	ret = sfc_sync(flash->sfc, &transfer);
	if(ret) {
		dev_err(flash->dev,"sfc_sync error ! %s %s %d\n",__FILE__,__func__,__LINE__);
		ret=-EIO;
	}
	return ret;
}

/* send 4byte command to enter 4byte mode */
static int set_4byte_mode_normal(struct sfc_flash *flash)
{
	int ret;
	ret = enter_4byte(flash);
	if (ret) {
		dev_err(flash->dev, "enter 4byte mode failed\n");
	}
	return ret;
}

/**
 * 1.send write enable command
 * 2.send 4byte command to enter 4byte mode
 **/
static int set_4byte_mode_wren(struct sfc_flash *flash)
{
	int ret;
	ret = write_enable(flash);
	if (ret) {
		dev_err(flash->dev, "enter 4byte mode failed\n");
		return ret;
	}

	ret = enter_4byte(flash);
	if (ret) {
		dev_err(flash->dev, "enter 4byte mode failed\n");
	}
	return ret;
}


static struct spi_nor_flash_ops nor_flash_ops;

static int noop(struct sfc_flash *flash)
{
	return 0;
}

int sfc_nor_get_special_ops(struct sfc_flash *flash)
{
	struct spinor_flashinfo *nor_info = flash->flash_info;
	struct spi_nor_info *spi_nor_info = nor_info->nor_flash_info;

	switch (spi_nor_info->quad_ops_mode) {
	case 0:
		nor_flash_ops.set_quad_mode = set_quad_mode_cmd;
		break;
	case 1:
		nor_flash_ops.set_quad_mode = set_quad_mode_reg;
		break;
	default:
		nor_flash_ops.set_quad_mode = noop;
		break;
	}

	switch (spi_nor_info->addr_ops_mode) {
	case 0:
		nor_flash_ops.set_4byte_mode = set_4byte_mode_normal;
		break;
	case 1:
		nor_flash_ops.set_4byte_mode = set_4byte_mode_wren;
		break;
	default:
		nor_flash_ops.set_4byte_mode = noop;
		break;
	}

	nor_info->nor_flash_ops = &nor_flash_ops;

	return 0;
}

