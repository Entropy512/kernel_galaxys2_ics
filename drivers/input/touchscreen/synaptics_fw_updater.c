/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Copyright (c) 2011 Synaptics, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/

#include "synaptics_fw.h"	/* FW. data file */
#include "synaptics_fw_updater.h"
#include "synaptics_reg.h"

#if 1
u8 firmware_image[16000];
u8 config_image[16000];
#endif

static int synaptics_ts_i2c_write(struct i2c_client *client,
	u8 addr, u8 *buf, u16 count)
{
	struct i2c_msg msg;
	int ret, i;
	u8 data[256];

	data[0] = addr;

	for (i = 1; i <= count; i++)
		data[i] = *buf++;

	msg.addr = client->addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = count + 1;
	msg.buf = data;

	ret = i2c_transfer(client->adapter, &msg, 1);

	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	   transmitted, else error code. */
	return (ret == 1) ? count : ret;
}

static int synaptics_ts_i2c_read(struct i2c_client *client,
	u8 addr, u8 *buf, u16 count)
{
	struct i2c_msg msg[2];
	int ret;

	msg[0].addr = client->addr;
	msg[0].flags = 0x00;
	msg[0].len = 1;
	msg[0].buf = (u8 *)&addr;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = count;
	msg[1].buf = buf;

	ret = i2c_transfer(client->adapter, msg, 2);

	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	   transmitted, else error code. */
	return (ret == 1) ? count : ret;
}

/* synaptics_setup scans the Page Description Table (PDT)
 * and sets up the necessary variables for the reflash process.
 * This function is a "slim" version of the
 * PDT scan function in PDT.c, since only F34 and F01 are needed for reflash.
 */
static void synaptics_setup(struct synaptics_ts_data *ts)
{
	struct synaptics_ts_fw_block *fw = ts->fw;
	u8 address;
	u8 buffer[6];

	for (address = 0xe9; address > 0xc0; address = address - 6) {
		synaptics_ts_i2c_read(ts->client, address, buffer, 6);

		switch (buffer[5]) {
		case 0x34:
			fw->f34_database = buffer[3];
			fw->f34_querybase = buffer[0];
			printk(KERN_DEBUG
				"[TSP] F34 Base - data : %u, query : %u\n",
				fw->f34_database, fw->f34_querybase);
			break;

		case 0x01:
			fw->f01_database = buffer[3];
			fw->f01_commandbase = buffer[1];
			printk(KERN_DEBUG
				"[TSP] F01 Base - data : %u, command : %u\n",
				fw->f01_database, fw->f01_commandbase);
			break;
		}
	}

	printk(KERN_DEBUG
		"[TSP] F34_FlashControl : %u\n",
		fw->f34_flashcontrol);

	fw->f34_reflash_blocknum
		= fw->f34_database;
	fw->f34_reflash_blockdata
		= fw->f34_database + 2;
	fw->f34_reflashquery_boot_id
		= fw->f34_querybase;
	fw->f34_reflashquery_flashpropertyquery
		= fw->f34_querybase + 2;
	fw->f34_reflashquery_fw_blocksize
		= fw->f34_querybase + 3;
	fw->f34_reflashquery_fw_blockcount
		= fw->f34_querybase + 5;
	fw->f34_reflashquery_config_blocksize
		= fw->f34_querybase + 3;
	fw->f34_reflashquery_config_blockcount
		= fw->f34_querybase + 7;

	fw->fw_imgdata = (u8 *)((&fw->fw_data[0]) + 0x100);
	fw->config_imgdata = (u8 *)(fw->fw_imgdata + fw->imagesize);
	fw->fw_version = (u32)(fw->fw_data[7]);

	switch (fw->fw_version) {
	case 2:
		fw->lock_imgdata = (u8 *)((&fw->fw_data[0]) + 0xD0);
		break;
	case 3:
	case 4:
		fw->lock_imgdata = (u8 *)((&fw->fw_data[0]) + 0xC0);
		break;
	case 5:
		fw->lock_imgdata = (u8 *)((&fw->fw_data[0]) + 0xB0);
		break;
	default:
		break;
	}
}

/* synaptics_fw_initialize sets up the reflahs process
 */
static void synaptics_fw_initialize(struct synaptics_ts_data *ts)
{
	struct synaptics_ts_fw_block *fw = ts->fw;
	u8 uData[2];

	uData[0] = 0x00;
	synaptics_ts_i2c_write(ts->client, 0xff, uData, 1);

	synaptics_setup(ts);

#if 1
	fw->fw_imgdata = &firmware_image[0];
	fw->config_imgdata = &config_image[0];
#endif

	synaptics_ts_i2c_read(ts->client,
		fw->f34_reflashquery_fw_blocksize, uData, 2);

	fw->fw_blocksize = uData[0] | (uData[1] << 8);
	printk(KERN_DEBUG "[TSP] %s - fw_blocksize : %u\n",
		__func__, fw->fw_blocksize);
}

/* synaptics_read_fw_info reads the F34 query registers and retrieves the block
 * size and count of the firmware section of the image to be reflashed
 */
static void synaptics_read_fw_info(struct synaptics_ts_data *ts)
{
	struct synaptics_ts_fw_block *fw = ts->fw;
	u8 uData[2];

#if 0	/* the block size was already gotten. */
	synaptics_ts_i2c_read(ts->client,
		fw->f34_reflashquery_fw_blocksize, uData, 2);
	fw->fw_blocksize = uData[0] | (uData[1] << 8);
#endif

	synaptics_ts_i2c_read(ts->client,
		fw->f34_reflashquery_fw_blockcount,
		uData, 2);
	fw->fw_blockcount = uData[0] | (uData[1] << 8);
	fw->imagesize = (u32)(fw->fw_blockcount * fw->fw_blocksize);
	printk(KERN_DEBUG "[TSP] %s - fw_blockcount : %u\n",
		__func__, fw->fw_blockcount);
	printk(KERN_DEBUG "[TSP] %s - imagesize : %u\n",
		__func__, fw->imagesize);
}

/* synaptics_read_config_info reads the F34 query registers
 * and retrieves the block size and count of the configuration section
 * of the image to be reflashed
 */
static void synaptics_read_config_info(struct synaptics_ts_data *ts)
{
	struct synaptics_ts_fw_block *fw = ts->fw;
	u8 uData[2];


	synaptics_ts_i2c_read(ts->client,
		fw->f34_reflashquery_config_blocksize,
		uData, 2);
	fw->config_blocksize = uData[0] | (uData[1] << 8);
	printk(KERN_DEBUG "[TSP] config_blocksize : %u\n",
		fw->config_blocksize);

	synaptics_ts_i2c_read(ts->client,
		fw->f34_reflashquery_config_blockcount,
		uData, 2);
	fw->config_blockcount = uData[0] | (uData[1] << 8);
	fw->config_imagesize =
		(u32)(fw->config_blockcount * fw->config_blocksize);
	printk(KERN_DEBUG "[TSP] config_blockcount : %u\n",
		fw->config_blockcount);
	printk(KERN_DEBUG "[TSP] config_imagesize : %u\n",
		fw->config_imagesize);
}

/* synaptics_read_bootload_id reads the F34 query registers
 * and retrieves the bootloader ID of the firmware
 */
static void synaptics_read_bootload_id(struct synaptics_ts_data *ts)
{
	struct synaptics_ts_fw_block *fw = ts->fw;
	u8 uData[2];

	synaptics_ts_i2c_read(ts->client,
		fw->f34_reflashquery_boot_id, uData, 2);
	fw->boot_id = uData[0] + uData[1] * 0x100;
	printk(KERN_DEBUG "[TSP] BootloadID : %u\n", fw->boot_id);
}

/* synaptics_write_bootload_id writes the bootloader ID
 * to the F34 data register to unlock the reflash process
 */
static void synaptics_write_bootload_id(struct synaptics_ts_data *ts)
{
	struct synaptics_ts_fw_block *fw = ts->fw;
	u8 uData[2];

	uData[0] = fw->boot_id % 0x100;
	uData[1] = fw->boot_id / 0x100;

	synaptics_ts_i2c_write(ts->client,
		fw->f34_reflash_blockdata, uData, 2);
}

/* synaptics_enable_flashing kicks off the reflash process
 */
static void synaptics_enable_flashing(struct synaptics_ts_data *ts)
{
	struct synaptics_ts_fw_block *fw = ts->fw;
	u8 uData;
	u8 uStatus;
	int cnt = 0;

	printk(KERN_DEBUG "[TSP] %s\n", __func__);

	/* Reflash is enabled by first reading the bootloader ID from
	   the firmware and write it back */
	synaptics_read_bootload_id(ts);
	synaptics_write_bootload_id(ts);

	/* Make sure Reflash is not already enabled */
	synaptics_ts_i2c_read(ts->client,
		fw->f34_flashcontrol, &uData, 1);
	while (((uData & 0x0f) != 0x00) && (cnt++ < 15)) {
		msleep(20);
		synaptics_ts_i2c_read(ts->client,
			fw->f34_flashcontrol, &uData, 1);
	}

	synaptics_ts_i2c_read(ts->client,
		fw->f01_database, &uStatus, 1);

	if ((uStatus & 0x40) == 0) {
		/* Write the "Enable Flash Programming command to
		F34 Control register Wait for ATTN and then clear the ATTN. */
		uData = 0x0f;
		synaptics_ts_i2c_write(ts->client,
			fw->f34_flashcontrol, &uData, 1);
		mdelay(300);
		synaptics_ts_i2c_read(ts->client,
			(fw->f01_database + 1), &uStatus, 1);

		/* Scan the PDT again to ensure all register offsets are
		correct */
		synaptics_setup(ts);

		/* Read the "Program Enabled" bit of the F34 Control register,
		and proceed only if the bit is set.*/
		synaptics_ts_i2c_read(ts->client,
		fw->f34_flashcontrol, &uData, 1);

		cnt = 0;
		while (((uData & 0x0f) != 0x00) && (cnt++ < 15)) {
			/* In practice, if uData!=0x80 happens for multiple
			counts, it indicates reflash is failed to be enabled,
			and program should quit */
			msleep(20);
			synaptics_ts_i2c_read(ts->client,
				fw->f34_flashcontrol, &uData, 1);
		}
	}
}

/* synaptics_wait_attn waits for ATTN to be asserted
 * within a certain time threshold.
 * The function also checks for the F34 "Program Enabled" bit and clear ATTN
 * accordingly.
 */
static void synaptics_wait_attn(struct synaptics_ts_data *ts)
{
	struct synaptics_ts_fw_block *fw = ts->fw;
	u8 uData;
	u8 uStatus;
	int cnt = 0;

	while (gpio_get_value(GPIO_TSP_INT) && cnt < 3) {
		msleep(20);
		cnt++;
	}
	cnt = 0;
	synaptics_ts_i2c_read(ts->client,
		fw->f34_flashcontrol, &uData, 1);
	while ((uData != 0x80) && (cnt++ < 5)) {
		msleep(20);
		synaptics_ts_i2c_read(ts->client,
			fw->f34_flashcontrol, &uData, 1);
	}
	synaptics_ts_i2c_read(ts->client,
		(fw->f01_database + 1), &uStatus, 1);
}

/* synaptics_program_config writes the configuration section of the image block
 * by block
 */
static void synaptics_program_config(struct synaptics_ts_data *ts)
{
	struct synaptics_ts_fw_block *fw = ts->fw;
	u8 uData[2];
	u8 *puData;
	unsigned short blockNum;

	printk(KERN_DEBUG "[TSP] Program Configuration Section...\n");

	puData = (u8 *) &fw->fw_data[0xb100];

	for (blockNum = 0; blockNum < fw->config_blockcount; blockNum++) {
		uData[0] = blockNum & 0xff;
		uData[1] = (blockNum & 0xff00) >> 8;

		/* Block by blcok, write the block number and data to
		the corresponding F34 data registers */
		synaptics_ts_i2c_write(ts->client,
		fw->f34_reflash_blocknum, uData, 2);
		synaptics_ts_i2c_write(ts->client,
			fw->f34_reflash_blockdata,
			puData, fw->config_blocksize);
		puData += fw->config_blocksize;

		/* Issue the "Write Configuration Block" command */
		uData[0] = 0x06;
		synaptics_ts_i2c_write(ts->client,
			fw->f34_flashcontrol, uData, 1);
		synaptics_wait_attn(ts);
		printk(KERN_DEBUG ".");
	}
}

/* synaptics_finalize_reflash finalizes the reflash process
*/
static void synaptics_finalize_reflash(struct synaptics_ts_data *ts)
{
	struct synaptics_ts_fw_block *fw = ts->fw;
	u8 uData;
	u8 uStatus;
	int cnt = 0;

	printk(KERN_DEBUG "[TSP] Finalizing Reflash..\n");

	/* Issue the "Reset" command to F01 command register to reset the chip
	 This command will also test the new firmware image and check if its is
	 valid */
	uData = 1;
	synaptics_ts_i2c_write(ts->client,
		fw->f01_commandbase, &uData, 1);

	mdelay(160);
	synaptics_ts_i2c_read(ts->client,
		fw->f01_database, &uData, 1);

	/* Sanity check that the reflash process is still enabled */
	synaptics_ts_i2c_read(ts->client,
		fw->f34_flashcontrol, &uStatus, 1);
	while (((uStatus & 0x0f) != 0x00) && (cnt++ < 15)) {
		msleep(20);
		synaptics_ts_i2c_read(ts->client,
			fw->f34_flashcontrol, &uStatus, 1);
	}
	synaptics_ts_i2c_read(ts->client,
		(fw->f01_database + 1), &uStatus, 1);

	synaptics_setup(ts);

	uData = 0;
	cnt = 0;

	/* Check if the "Program Enabled" bit in F01 data register is cleared
	 Reflash is completed, and the image passes testing when the bit is
	 cleared */
	synaptics_ts_i2c_read(ts->client, fw->f01_database, &uData, 1);
	while (((uData & 0x40) != 0) && (cnt++ < 15)) {
		msleep(20);
		synaptics_ts_i2c_read(ts->client, fw->f01_database, &uData, 1);
	}

	/* Rescan PDT the update any changed register offsets */
	synaptics_setup(ts);

	printk(KERN_DEBUG "[TSP] Reflash Completed. Please reboot.\n");
}

/* synaptics_fw_write writes the firmware section of the image block by
 * block
 */
static void synaptics_fw_write(struct synaptics_ts_data *ts)
{
	struct synaptics_ts_fw_block *fw = ts->fw;
	u8 *puFirmwareData;
	u8 uData[2];
	unsigned short blockNum;

	printk(KERN_DEBUG "[TSP] %s\n", __func__);

	puFirmwareData = (u8 *) &fw->fw_data[0x100];

	for (blockNum = 0; blockNum < fw->fw_blockcount; ++blockNum) {
		printk(KERN_DEBUG "[TSP] blockNum : %u\n", blockNum);
		/* Block by blcok, write the block number and data to
		the corresponding F34 data registers */
		uData[0] = blockNum & 0xff;
		uData[1] = (blockNum & 0xff00) >> 8;
		synaptics_ts_i2c_write(ts->client,
			fw->f34_reflash_blocknum, uData, 2);

		synaptics_ts_i2c_write(ts->client,
			fw->f34_reflash_blockdata, puFirmwareData,
			fw->fw_blocksize);
		puFirmwareData += fw->fw_blocksize;

		/* Issue the "Write Firmware Block" command */
		uData[0] = 2;
		synaptics_ts_i2c_write(ts->client,
			fw->f34_flashcontrol, uData, 1);

		synaptics_wait_attn(ts);
	}
}

/* synaptics_program_fw prepares the firmware writing process
*/
static void synaptics_program_fw(struct synaptics_ts_data *ts)
{
	struct synaptics_ts_fw_block *fw = ts->fw;
	u8 uData;

	printk(KERN_DEBUG "[TSP] %s\n", __func__);

	synaptics_read_bootload_id(ts);
	synaptics_write_bootload_id(ts);

	uData = 3;
	synaptics_ts_i2c_write(ts->client, fw->f34_flashcontrol, &uData, 1);

	synaptics_wait_attn(ts);
	synaptics_fw_write(ts);
}

#if 0
/* erase_config_block erases the config block
*/
static void erase_config_block(struct synaptics_ts_data *ts)
{
	struct synaptics_ts_fw_block *fw = ts->fw;
	u8 uData;

	printk(KERN_DEBUG "[TSP] %s\n", __func__);
	/* Erase of config block is done by first entering into
	bootloader mode */
	synaptics_read_bootload_id(ts);
	synaptics_write_bootload_id(ts);

	/* Command 7 to erase config block */
	uData = 7;
	synaptics_ts_i2c_write(ts->client, fw->f34_flashcontrol, &uData, 1);

	synaptics_wait_attn(ts);
}
#endif

static u8 get_fw_version_address(struct synaptics_ts_data *ts)
{
	u8 ret = 0;
	u8 address;
	u8 buffer[6];

	for (address = 0xe9; address > 0xd0; address -= 6) {
		synaptics_ts_i2c_read(ts->client, address, buffer, 6);

		if (!buffer[5])
			break;
		switch (buffer[5]) {
		case 0x34:
			ret = buffer[2];
			break;
		}
	}

	return ret;
}

void synaptics_fw_updater(struct synaptics_ts_data *ts, u8 *fw_data)
{
	struct synaptics_ts_fw_block *fw;
	bool update = true;
	u8 addr = get_fw_version_address(ts);
	u8 buf[4], fw_version[4];

	fw = kzalloc(sizeof(struct synaptics_ts_data), GFP_KERNEL);
	ts->fw = fw;

	if (NULL == fw_data) {
		int i = 0;
		fw->fw_data = (u8 *)synaptics_fw;
		for (i = 0; i < 4; i++)
			fw_version[i] = synaptics_fw[0xb100 + i];

		synaptics_ts_i2c_read(ts->client, addr, buf, 4);
		printk(KERN_DEBUG "[TSP] IC FW. : [%s], new FW. : [%s]\n",
			(char *)buf, (char *)fw_version);
		if (strncmp((char *)fw_version, (char *)buf, 4) == 0)
			update = false;
	} else
		fw->fw_data = fw_data;

	if (update) {
		synaptics_fw_initialize(ts);
		synaptics_read_config_info(ts);
		synaptics_read_fw_info(ts);
		fw->f34_flashcontrol = fw->f34_database
			+ fw->fw_blocksize + 2;
		printk(KERN_DEBUG
			"[TSP] F34_FlashControl : %u\n",
			fw->f34_flashcontrol);

		synaptics_enable_flashing(ts);
		synaptics_program_fw(ts);
		synaptics_program_config(ts);
		synaptics_finalize_reflash(ts);
	}
}

