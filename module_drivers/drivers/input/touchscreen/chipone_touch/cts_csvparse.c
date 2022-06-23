/*
 * Huawei Touchscreen Driver
 *
 * Copyright (c) 2012-2019 Huawei Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include "cts_platform.h"

#define SUPPORT_UNIQUE_TEST 1
#define STRTOL_LEN 10
#define UNIQUE_TEST_FAIL (-10)
#define MAX_ROW_SIZE 1024

static void copy_this_line(char *dest, char *src)
{
	char *copy_from = NULL;
	char *copy_to = NULL;

	copy_from = src;
	copy_to = dest;
	do {
		*copy_to = *copy_from;
		copy_from++;
		copy_to++;
	} while ((*copy_from != '\n') && (*copy_from != '\r') &&
		(*copy_from != '\0'));
	*copy_to = '\0';
}

static void goto_next_line(char **ptr)
{
	do {
		*ptr = *ptr + 1;
	} while (**ptr != '\n' && **ptr != '\0');
	if (**ptr == '\0')
		return;
	*ptr = *ptr + 1;
}

static void parse_valid_data(const char *buf_start, loff_t buf_size,
				char *ptr, int32_t* data, int rows)
{
	int i;
	int j = 0;
	char *token = NULL;
	char *tok_ptr = NULL;
	char *row_data = NULL;

	if (!ptr) {
		cts_err("%s, ptr is NULL\n", __func__);
		return;
	}
	if (!data) {
		cts_err("%s, data is NULL\n", __func__);
		return;
	}
	row_data = kzalloc(MAX_ROW_SIZE * sizeof(char), GFP_KERNEL);
	if (!row_data) {
		cts_err("%s: kzalloc row_data failed.\n", __func__);
		return;
	}

	for (i = 0; i < rows; i++) {
		// copy this line to row_data buffer
		memset(row_data, 0, MAX_ROW_SIZE * sizeof(char));
		copy_this_line(row_data, ptr);
		tok_ptr = row_data;
		while ((token = strsep(&tok_ptr, ", \t\n\r\0"))) {
			if (strlen(token) == 0)
				continue;

			data[j] = (int32_t)simple_strtol(token, NULL, STRTOL_LEN);
			j ++;
		}
		goto_next_line(&ptr);	// next row
		if (!ptr || (strlen(ptr) == 0) ||
			(ptr >= (buf_start + buf_size))) {
			cts_info("invalid ptr, return\n");
			break;
		}
	}
	kfree(row_data);
	row_data = NULL;
	return;
}

static void print_data(char *target_name, int32_t *data, int rows, int columns)
{
	int i;
	int j;
	(void)target_name;

	if (data == NULL) {
		cts_err("rawdata is NULL\n");
		return;
	}

	for (i = 0; i < rows; i++) {
		for (j = 0; j < columns; j++)
			printk("\t%d", data[i * columns + j]);
		printk("\n");
	}
}

/*lint -save -e* */
int ts_kit_parse_csvfile(char *file_path, char *target_name,
			int32_t *data, int rows, int columns)
{
	struct file *fp = NULL;
	struct kstat stat;
	int ret;
	int32_t read_ret;
	char *buf = NULL;
	char *ptr = NULL;
	mm_segment_t org_fs;
	loff_t pos = 0;

	org_fs = get_fs();
	set_fs(KERNEL_DS);

	if (file_path == NULL) {
		cts_err("file path pointer is NULL\n");
		ret = -EPERM;
		goto exit_free;
	}

	if (target_name == NULL) {
		cts_err("target path pointer is NULL\n");
		ret = -EPERM;
		goto exit_free;
	}

	cts_info("%s, file name is %s, target is %s\n",
		__func__, file_path, target_name);

	fp = filp_open(file_path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		cts_err("%s, filp_open error, file name is %s\n",
			__func__, file_path);
		ret = -EPERM;
		goto exit_free;
	}

	ret = vfs_stat(file_path, &stat);
	if (ret) {
		cts_err("%s, failed to get file stat\n", __func__);
		ret = -ENOENT;
		goto exit_free;
	}

	buf = (char *)kzalloc(stat.size + 1, GFP_KERNEL);
	if (buf == NULL) {
		cts_err("%s: kzalloc %lld bytes failed\n",
			__func__, stat.size);
		ret = -ESRCH;
		goto exit_free;
	}

	read_ret = vfs_read(fp, buf, stat.size, &pos);
	if (read_ret > 0) {
		buf[stat.size] = '\0';
		ptr = buf;
		ptr = strstr(ptr, target_name);
		if (ptr == NULL) {
			cts_err("%s: load %s failed 1!\n",
				__func__, target_name);
			ret = -EINTR;
			goto exit_free;
		}
		/* walk thru this line */
		goto_next_line(&ptr);
		if ((ptr == NULL) || (strlen(ptr) == 0)) {
			cts_err("%s: load %s failed 2!\n",
				__func__, target_name);
			ret = -EIO;
			goto exit_free;
		}

		/* analyze the data */
		if (data) {
			parse_valid_data(buf, stat.size, ptr, data, rows);
			print_data(target_name, data,  rows, columns);
		} else {
			cts_err("%s: load %s failed 3!\n",
				__func__, target_name);
			ret = -EINTR;
			goto exit_free;
		}
	} else {
		cts_err("%s: ret=%d,read_ret=%d, buf=%pK, stat.size=%lld\n",
			__func__, ret, read_ret, buf, stat.size);
		ret = -ENXIO;
		goto exit_free;
	}
	ret = 0;
 exit_free:
	cts_info("%s exit free\n", __func__);
	set_fs(org_fs);
	if (buf) {
		cts_info("kfree buf\n");
		kfree(buf);
		buf = NULL;
	}
/* fp open fail not means fp is NULL, so free fp may cause Uncertainty */
	if (!IS_ERR_OR_NULL(fp)) {
		cts_info("filp close\n");
		filp_close(fp, NULL);
		fp = NULL;
	}
	return ret;
}

EXPORT_SYMBOL(ts_kit_parse_csvfile);
/* lint -restore */
