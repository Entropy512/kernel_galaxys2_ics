/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 * Contact: Jiyoung Shin<idon.shin@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/videonode.h>
#include <media/exynos_mc.h>
#include <linux/cma.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/scatterlist.h>
#include <linux/videodev2_samsung.h>

#include "fimc-is-core.h"
#include "fimc-is-helper.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-misc.h"

static int debug=1;
#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
extern const struct fimc_is_vb2 fimc_is_vb2_cma;
#elif defined(CONFIG_VIDEOBUF2_ION)
extern const struct fimc_is_vb2 fimc_is_vb2_ion;
struct vb2_buffer *is_vb;
void *buf_start;

extern const struct v4l2_file_operations fimc_is_scalerc_video_fops;
extern const struct v4l2_ioctl_ops fimc_is_scalerc_video_ioctl_ops;
extern const struct vb2_ops fimc_is_scalerc_qops;

extern const struct v4l2_file_operations fimc_is_scalerp_video_fops;
extern const struct v4l2_ioctl_ops fimc_is_scalerp_video_ioctl_ops;
extern const struct vb2_ops fimc_is_scalerp_qops;

extern const struct v4l2_file_operations fimc_is_3dnr_video_fops;
extern const struct v4l2_ioctl_ops fimc_is_3dnr_video_ioctl_ops;
extern const struct vb2_ops fimc_is_3dnr_qops;

struct vb2_ion_conf {
	struct device		*dev;
	const char		*name;

	struct ion_client	*client;

	unsigned long		align;
	bool			contig;
	bool			sharable;
	bool			cacheable;
	bool			use_mmu;
	atomic_t		mmu_enable;

	spinlock_t		slock;
};

struct vb2_ion_buf {
	struct vm_area_struct		*vma;
	struct vb2_ion_conf		*conf;
	struct vb2_vmarea_handler	handler;

	struct ion_handle		*handle;	/* Kernel space */

	dma_addr_t			kva;
	dma_addr_t			dva;
	size_t				offset;
	unsigned long			size;

	struct scatterlist		*sg;
	int				nents;

	atomic_t			ref;

	bool				cacheable;
};

#endif

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Enable module debug trace. Set to 1 to enable.");

static struct fimc_is_dev *to_fimc_is_dev_from_front_dev(struct fimc_is_front_dev *front_dev)
{
	return container_of(front_dev, struct fimc_is_dev, front);
}

static struct fimc_is_sensor_dev *to_fimc_is_sensor_dev(struct v4l2_subdev *sdev)
{
	return container_of(sdev, struct fimc_is_sensor_dev, sd);
}

static struct fimc_is_front_dev *to_fimc_is_front_dev(struct v4l2_subdev *sdev)
{
	return container_of(sdev, struct fimc_is_front_dev, sd);
}

static struct fimc_is_back_dev *to_fimc_is_back_dev(struct v4l2_subdev *sdev)
{
	return container_of(sdev, struct fimc_is_back_dev, sd);
}

static int fimc_is_sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
void fimc_is_mem_cache_clean(const void *start_addr, unsigned long size)
{
	unsigned long paddr;

	dmac_map_area(start_addr, size, DMA_TO_DEVICE);
	/*
	 * virtual & phsical addrees mapped directly, so we can convert
	 * the address just using offset
	 */
	paddr = __pa((unsigned long)start_addr);
	outer_clean_range(paddr, paddr + size);
}

void fimc_is_mem_cache_inv(const void *start_addr, unsigned long size)
{
	unsigned long paddr;
	paddr = __pa((unsigned long)start_addr);
	outer_inv_range(paddr, paddr + size);
	dmac_unmap_area(start_addr, size, DMA_FROM_DEVICE);
}

int fimc_is_init_mem(struct fimc_is_dev *dev)
{
	struct cma_info mem_info;
	char			cma_name[16];
	int err;

	printk(KERN_DEBUG "fimc_is_init_mem - ION\n");
	sprintf(cma_name, "%s%d", "fimc_is", 0);
	err = cma_info(&mem_info, &dev->pdev->dev, 0);
	printk(KERN_INFO "%s : [cma_info] start_addr : 0x%x, end_addr : 0x%x, "
			"total_size : 0x%x, free_size : 0x%x\n",
			__func__, mem_info.lower_bound, mem_info.upper_bound,
			mem_info.total_size, mem_info.free_size);
	if (err) {
		dev_err(&dev->pdev->dev, "%s: get cma info failed\n", __func__);
		return -EINVAL;
	}
	dev->mem.size = FIMC_IS_A5_MEM_SIZE;
	dev->mem.base = (dma_addr_t)cma_alloc
		(&dev->pdev->dev, cma_name, (size_t)dev->mem.size, 0);
	dev->is_p_region =
		(struct is_region *)(phys_to_virt(dev->mem.base +
				FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE));
	memset((void *)dev->is_p_region, 0,
		(unsigned long)sizeof(struct is_region));
	fimc_is_mem_cache_clean((void *)dev->is_p_region, FIMC_IS_REGION_SIZE+1);

	printk(KERN_INFO "ctrl->mem.size = 0x%x\n", dev->mem.size);
	printk(KERN_INFO "ctrl->mem.base = 0x%x\n", dev->mem.base);

	return 0;
}
#elif defined(CONFIG_VIDEOBUF2_ION)

void fimc_is_mem_init_mem_cleanup(void *alloc_ctxes)
{
	vb2_ion_cleanup(alloc_ctxes);
}

void fimc_is_mem_resume(void *alloc_ctxes)
{
	vb2_ion_resume(alloc_ctxes);
}

void fimc_is_mem_suspend(void *alloc_ctxes)
{
	vb2_ion_suspend(alloc_ctxes);
}

int fimc_is_cache_flush(struct vb2_buffer *vb,
				const void *start_addr, unsigned long size)
{
	return vb2_ion_cache_flush(vb, 1);
}

int fimc_is_cache_inv(struct vb2_buffer *vb,
				const void *start_addr, unsigned long size)
{
	return vb2_ion_cache_inv(vb, 1);
}

/* Allocate firmware */
int fimc_is_alloc_firmware(struct fimc_is_dev *dev)
{
	void *fimc_is_bitproc_buf;
	dbg("Allocating memory for FIMC-IS firmware.\n");

	fimc_is_bitproc_buf =
		dev->vb2->ops->alloc(dev->alloc_ctx, FIMC_IS_A5_MEM_SIZE+FIMC_IS_TDNR_MEM_SIZE);
	if (IS_ERR(fimc_is_bitproc_buf)) {
		fimc_is_bitproc_buf = 0;
		printk(KERN_ERR "Allocating bitprocessor buffer failed\n");
		return -ENOMEM;
	}

	dev->mem.dvaddr = (size_t)dev->vb2->ops->cookie(fimc_is_bitproc_buf);
	if (dev->mem.dvaddr  & FIMC_IS_FW_BASE_MASK) {
		err("The base memory is not aligned to 64MB.\n");
		dev->vb2->ops->put(fimc_is_bitproc_buf);
		dev->mem.dvaddr = 0;
		fimc_is_bitproc_buf = 0;
		return -EIO;
	}
	dbg("Device vaddr = %08x , size = %08x\n",
				dev->mem.dvaddr, FIMC_IS_A5_MEM_SIZE);

	dev->mem.kvaddr = dev->vb2->ops->vaddr(fimc_is_bitproc_buf);
	if (!dev->mem.kvaddr) {
		err("Bitprocessor memory remap failed\n");
		dev->vb2->ops->put(fimc_is_bitproc_buf);
		dev->mem.dvaddr = 0;
		fimc_is_bitproc_buf = 0;
		return -EIO;
	}
	dbg("Virtual address for FW: %08lx\n",
			(long unsigned int)dev->mem.kvaddr);
	dbg("Physical address for FW: %08lx\n",
			(long unsigned int)virt_to_phys(dev->mem.kvaddr));
	dev->mem.bitproc_buf = fimc_is_bitproc_buf;
	dev->mem.vb2_buf.planes[0].mem_priv = fimc_is_bitproc_buf;

	is_vb = &dev->mem.vb2_buf;
	buf_start = dev->mem.kvaddr;
	return 0;
}

void fimc_is_mem_cache_clean(const void *start_addr, unsigned long size)
{
	struct vb2_ion_buf *buf;
	off_t offset;

	if (start_addr < buf_start) {
		err("Start address error\n");
		return;
	}
	size--;

	offset = start_addr - buf_start;

	buf = (struct vb2_ion_buf *)is_vb->planes[0].mem_priv;
	dma_sync_sg_for_device(buf->conf->dev, buf->sg, buf->nents,
                                                       DMA_BIDIRECTIONAL);
}

void fimc_is_mem_cache_inv(const void *start_addr, unsigned long size)
{
	struct vb2_ion_buf *buf;
	struct scatterlist *sg;
	int i;
	off_t offset;

	if (start_addr < buf_start) {
		err("Start address error\n");
		return;
	}

	offset = start_addr - buf_start;

	buf = (struct vb2_ion_buf *)is_vb->planes[0].mem_priv;
	for_each_sg(buf->sg, sg, buf->nents, i) {
		phys_addr_t start, end;

		if (offset >= sg_dma_len(sg)) {
			offset -= sg_dma_len(sg);
			continue;
		}

		start = sg_phys(sg);
		end = start + sg_dma_len(sg);

		dmac_flush_range(phys_to_virt(start),
				 phys_to_virt(end));
		outer_flush_range(start, end);	/* L2 */
		if (size == 0)
			break;
	}
}

int fimc_is_init_mem(struct fimc_is_dev *dev)
{
	int ret;

	printk(KERN_DEBUG "fimc_is_init_mem - ION\n");
	ret = fimc_is_alloc_firmware(dev);
	if (ret) {
		err("Couldn't alloc for FIMC-IS firmware\n");
		return -EINVAL;
	}

	memset(dev->mem.kvaddr, 0, FIMC_IS_A5_MEM_SIZE+FIMC_IS_TDNR_MEM_SIZE);
	dev->is_p_region =
		(struct is_region *)(dev->mem.kvaddr +
			FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE);

	dev->mem.dvaddr_shared = (unsigned char *)dev->mem.dvaddr +
			((unsigned char *)&dev->is_p_region->shared[0] - dev->mem.kvaddr);
	dev->mem.kvaddr_shared = dev->mem.kvaddr +
			((unsigned char *)&dev->is_p_region->shared[0] - dev->mem.kvaddr);

	if (fimc_is_cache_flush(&dev->mem.vb2_buf,
			(void *)dev->is_p_region, IS_PARAM_SIZE)) {
		err("fimc_is_cache_flush-Err\n");
		return -EINVAL;
	}
	printk("fimc_is_init_mem3\n");
	return 0;
}
#endif

static int fimc_is_request_firmware(struct fimc_is_dev *dev)
{
	int ret;
	struct firmware *fw_blob;

	ret = request_firmware((const struct firmware **)&fw_blob,
				FIMC_IS_FW, &dev->pdev->dev);
	if (ret) {
		dev_err(&dev->pdev->dev,
			"could not load firmware (err=%d)\n", ret);
		return -EINVAL;
	}

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
		memcpy((void *)phys_to_virt(dev->mem.base),
				fw_blob->data, fw_blob->size);
		fimc_is_mem_cache_clean((void *)phys_to_virt(dev->mem.base),
							fw_blob->size + 1);
#elif defined(CONFIG_VIDEOBUF2_ION)
		if (dev->mem.bitproc_buf == 0) {
			err("failed to load FIMC-IS F/W\n");
		} else {
			memcpy(dev->mem.kvaddr, fw_blob->data, fw_blob->size);
			fimc_is_mem_cache_clean(
				(void *)dev->mem.kvaddr, fw_blob->size + 1);
		}
#endif

	dbg("FIMC_IS F/W loaded successfully - size:%d\n",
						fw_blob->size);
	release_firmware(fw_blob);

	return ret;
}

int fimc_is_load_fw(struct  fimc_is_dev *dev)
{
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);
	if (test_bit(IS_ST_IDLE, &dev->state)) {
		/* 1. Load IS firmware */
		ret = fimc_is_request_firmware(dev);
		if (ret) {
			printk(KERN_ERR "failed to fimc_is_request_firmware (%d)\n", ret);
			return -EINVAL;
		}

		set_bit(IS_ST_PWR_ON, &dev->state);

		/* 3. A5 power on */
		clear_bit(IS_ST_FW_DOWNLOADED, &dev->state);
		fimc_is_hw_a5_power(dev, 1);
		ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_FW_DOWNLOADED, &dev->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		if (!ret) {
			printk(KERN_ERR "wait timeout A5 power on: %s\n", __func__);
			return -EINVAL;
		}
		dbg("fimc_is_load_fw end\n");
	} else {
		dbg("IS FW was loaded before\n");
	}
	return 0;
}

static int fimc_is_load_setfile(struct fimc_is_dev *dev)
{
	int ret;
	struct firmware *fw_blob;

	ret = request_firmware((const struct firmware **)&fw_blob,
				FIMC_IS_SETFILE, &dev->pdev->dev);
	if (ret) {
		dev_err(&dev->pdev->dev,
			"could not load firmware (err=%d)\n", ret);
		return -EINVAL;
	}

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
	memcpy((void *)phys_to_virt(dev->mem.base + dev->setfile.base),
		fw_blob->data, fw_blob->size);
	fimc_is_mem_cache_clean(
		(void *)phys_to_virt(dev->mem.base + dev->setfile.base),
		fw_blob->size + 1);
#elif defined(CONFIG_VIDEOBUF2_ION)
	if (dev->mem.bitproc_buf == 0) {
		err("failed to load FIMC-IS F/W\n");
	} else {
		memcpy((dev->mem.kvaddr + dev->setfile.base),
					fw_blob->data, fw_blob->size);
		fimc_is_mem_cache_clean((void *)dev->mem.kvaddr,
					fw_blob->size + 1);
	}
#endif
	dev->setfile.state = 1;
	dbg("FIMC_IS setfile loaded successfully - size:%d\n",
							fw_blob->size);
	release_firmware(fw_blob);

	dbg("A5 mem base  = 0x%08x\n", dev->mem.base);
	dbg("Setfile base  = 0x%08x\n", dev->setfile.base);

	return ret;
}

int fimc_is_init_set(struct fimc_is_dev *dev , u32 val)
{
	int ret;

	fimc_is_hw_diable_wdt(dev);
	dev->sensor.sensor_type = val;
	dev->sensor.id = 0;
	dbg("fimc_is_init\n");
	if (test_bit(IS_ST_FW_DOWNLOADED, &dev->state)) {
		/* Init sequence 1: Open sensor */
		dbg("v4l2 : open sensor : %d\n", val);

		clear_bit(IS_ST_INIT_PREVIEW_STILL, &dev->state);
		fimc_is_hw_open_sensor(dev, dev->sensor.id, val);

		ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_INIT_PREVIEW_STILL, &dev->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		if (!ret) {
			dev_err(&dev->pdev->dev,
				"wait timeout:%s\n", __func__);
			return -EBUSY;
		}

		if(dev->is_p_region->shared[MAX_SHARED_COUNT-1]  != MAGIC_NUMBER)
			dev_err(&dev->pdev->dev,	"MAGIC NUMBER error\n");

		ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_SET_FILE, &dev->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		if (!ret) {
			dev_err(&dev->pdev->dev,
				"wait timeout:%s\n", __func__);
			return -EBUSY;
		}

		dbg(" Load setfile received\n");
		fimc_is_load_setfile(dev);
		dbg(" fimc_is_load_setfile end\n");
		fimc_is_hw_wait_intmsr0_intmsd0(dev);
		dbg(" fimc_is_hw_wait_intmsr0_intmsd0 end\n");
		fimc_is_hw_set_load_setfile(dev);
		fimc_is_hw_set_intgr0_gd0(dev);
		dbg("v4l2 : Load set file end\n");
		/* Debug only */
		dbg("Parameter region addr = 0x%08x\n",
			virt_to_phys(dev->is_p_region));
		dbg("ISP region addr = 0x%08x\n",
			virt_to_phys(&dev->is_p_region->parameter.isp));
		dbg("scalerc region addr = 0x%08x\n",
			virt_to_phys(&dev->is_p_region->parameter.scalerc));
		dbg("tdnr region addr = 0x%08x\n",
			virt_to_phys(&dev->is_p_region->parameter.tdnr));
		dbg("FN[0] addr = 0x%x\n",
			virt_to_phys(&dev->is_p_region->header[0]
							.frame_number));
		dbg("FN[1] addr = 0x%x\n",
			virt_to_phys(&dev->is_p_region->header[1]
							.frame_number));
		dbg("FN[2] addr = 0x%x\n",
			virt_to_phys(&dev->is_p_region->header[2]
							.frame_number));
		dbg("FN[3] addr = 0x%x\n",
			virt_to_phys(&dev->is_p_region->header[3]
							.frame_number));
		dev->frame_count = 0;

		dbg("Stream Off\n");
		clear_bit(IS_ST_STREAM_OFF, &dev->state);
		fimc_is_hw_set_stream(dev, 0); /*stream off */
		ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_STREAM_OFF, &dev->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		if (!ret) {
			dev_err(&dev->pdev->dev,
				"wait timeout : %s\n", __func__);
			return -EBUSY;
		}
		clear_bit(IS_ST_RUN, &dev->state);

		/* 1. */
		dbg("Default setting : preview_still\n");
		dev->scenario_id = ISS_PREVIEW_STILL;
		fimc_is_hw_set_init(dev);

		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);

		dbg(" phy :: &dev->is_p_region->shared[447]= 0x%08x\n",
			virt_to_phys( &dev->is_p_region->shared[447]));

		/* 1 */
		fimc_is_hw_set_param(dev);
		ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_INIT_PREVIEW_VIDEO, &dev->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		if (!ret) {
			dev_err(&dev->pdev->dev,
				"wait timeout : %s\n", __func__);
			return -EBUSY;
		}
#if 0 /* FW restrction, will be updated */

		/* 2. */
		dbg("Default setting : preview_video\n");
		dev->scenario_id = ISS_PREVIEW_VIDEO;
		fimc_is_hw_set_init(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_INIT_CAPTURE_STILL, &dev->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		if (!ret) {
			dev_err(&dev->pdev->dev,
				"wait timeout : %s\n", __func__);
			return -EBUSY;
		}
		/* 3. */
		dbg("Default setting : capture_still\n");
		dev->scenario_id = ISS_CAPTURE_STILL;
		fimc_is_hw_set_init(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_INIT_CAPTURE_VIDEO, &dev->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		if (!ret) {
			dev_err(&dev->pdev->dev,
				"wait timeout : %s\n", __func__);
			return -EBUSY;
		}
		/* 4. */
		dbg("Default setting : capture_video\n");
		dev->scenario_id = ISS_CAPTURE_VIDEO;
		fimc_is_hw_set_init(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_RUN, &dev->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		if (!ret) {
			dev_err(&dev->pdev->dev,
				"wait timeout : %s\n", __func__);
			return -EBUSY;
		}
#endif
		clear_bit(IS_ST_STREAM_OFF, &dev->state);
		set_bit(IS_ST_RUN, &dev->state);

		dbg("Init sequence completed!! Ready to use\n");
	}

	return 0;
}

static int fimc_is_front_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct fimc_is_front_dev *front_dev = to_fimc_is_front_dev(sd);
	struct fimc_is_dev *isp = to_fimc_is_dev_from_front_dev(front_dev);
	int ret;

	if (enable) {
		printk(KERN_INFO "fimc_is_front_s_stream : ON\n");
	} else {
		printk(KERN_INFO "fimc_is_front_s_stream : OFF\n");
		fimc_is_hw_subip_poweroff(isp);
		ret = wait_event_timeout(isp->irq_queue,
			test_bit(FIMC_IS_PWR_ST_POWEROFF, &isp->power),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		if (!ret) {
			dev_err(&isp->pdev->dev,
				"wait timeout : %s\n", __func__);
			return -EBUSY;
		}
		fimc_is_hw_a5_power(isp, 0);
		isp->state = 0;
		isp->pipe_state = 0;
		stop_mipi_csi();
		stop_fimc_lite();
		if (isp->pdata->clk_off) {
			/* isp->pdata->clk_off(isp->pdev); */
		} else {
			printk(KERN_ERR "#### failed to Clock On ####\n");
			return -EINVAL;
		}
		set_bit(IS_ST_IDLE , &isp->state);
		printk(KERN_DEBUG "state(%d), pipe_state(%d)\n", (int)isp->state, (int)isp->pipe_state);
	}

	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_back_s_stream(struct v4l2_subdev *sd, int enable)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_subdev_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_subdev_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_subdev_get_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_crop *crop)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_subdev_set_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_crop *crop)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_front_subdev_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_front_subdev_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_front_subdev_get_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_crop *crop)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_front_subdev_set_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_crop *crop)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_back_subdev_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_back_subdev_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_back_subdev_get_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_crop *crop)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_back_subdev_set_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_crop *crop)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_front_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_front_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}
static int fimc_is_back_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_back_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static struct v4l2_subdev_pad_ops fimc_is_sensor_pad_ops = {
	.get_fmt	= fimc_is_sensor_subdev_get_fmt,
	.set_fmt	= fimc_is_sensor_subdev_set_fmt,
	.get_crop	= fimc_is_sensor_subdev_get_crop,
	.set_crop	= fimc_is_sensor_subdev_set_crop,
};

static struct v4l2_subdev_pad_ops fimc_is_front_pad_ops = {
	.get_fmt	= fimc_is_front_subdev_get_fmt,
	.set_fmt	= fimc_is_front_subdev_set_fmt,
	.get_crop	= fimc_is_front_subdev_get_crop,
	.set_crop	= fimc_is_front_subdev_set_crop,
};

static struct v4l2_subdev_pad_ops fimc_is_back_pad_ops = {
	.get_fmt	= fimc_is_back_subdev_get_fmt,
	.set_fmt	= fimc_is_back_subdev_set_fmt,
	.get_crop	= fimc_is_back_subdev_get_crop,
	.set_crop	= fimc_is_back_subdev_set_crop,
};

static struct v4l2_subdev_video_ops fimc_is_sensor_video_ops = {
	.s_stream	= fimc_is_sensor_s_stream,
};

static struct v4l2_subdev_video_ops fimc_is_front_video_ops = {
	.s_stream	= fimc_is_front_s_stream,
};

static struct v4l2_subdev_video_ops fimc_is_back_video_ops = {
	.s_stream	= fimc_is_back_s_stream,
};

static struct v4l2_subdev_core_ops fimc_is_sensor_core_ops = {
	.s_ctrl	= fimc_is_sensor_s_ctrl,
	.g_ctrl = fimc_is_sensor_g_ctrl,
};

static struct v4l2_subdev_core_ops fimc_is_front_core_ops = {
	.s_ctrl	= fimc_is_front_s_ctrl,
	.g_ctrl = fimc_is_front_g_ctrl,
};

static struct v4l2_subdev_core_ops fimc_is_back_core_ops = {
	.s_ctrl	= fimc_is_back_s_ctrl,
	.g_ctrl = fimc_is_back_g_ctrl,
};

static struct v4l2_subdev_ops fimc_is_sensor_subdev_ops = {
	.pad	= &fimc_is_sensor_pad_ops,
	.video	= &fimc_is_sensor_video_ops,
	.core	= &fimc_is_sensor_core_ops,
};

static struct v4l2_subdev_ops fimc_is_front_subdev_ops = {
	.pad	= &fimc_is_front_pad_ops,
	.video	= &fimc_is_front_video_ops,
	.core	= &fimc_is_front_core_ops,
};

static struct v4l2_subdev_ops fimc_is_back_subdev_ops = {
	.pad	= &fimc_is_back_pad_ops,
	.video	= &fimc_is_back_video_ops,
	.core	= &fimc_is_back_core_ops,
};

static int fimc_is_sensor_init_formats(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_subdev_close(struct v4l2_subdev *sd,
			      struct v4l2_subdev_fh *fh)
{
	printk(KERN_DEBUG  "%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_subdev_registered(struct v4l2_subdev *sd)
{
	printk(KERN_DEBUG  "%s\n", __func__);
	return 0;
}

static void fimc_is_sensor_subdev_unregistered(struct v4l2_subdev *sd)
{
	printk(KERN_DEBUG  "%s\n", __func__);
}

static int fimc_is_front_init_formats(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	printk(KERN_DEBUG  "%s\n", __func__);
	return 0;
}

static int fimc_is_front_subdev_close(struct v4l2_subdev *sd,
			      struct v4l2_subdev_fh *fh)
{
	printk(KERN_DEBUG  "%s\n", __func__);
	return 0;
}

static int fimc_is_front_subdev_registered(struct v4l2_subdev *sd)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static void fimc_is_front_subdev_unregistered(struct v4l2_subdev *sd)
{
	printk(KERN_DEBUG  "%s\n", __func__);
}

static int fimc_is_back_init_formats(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	printk(KERN_DEBUG  "%s\n", __func__);
	return 0;
}

static int fimc_is_back_subdev_close(struct v4l2_subdev *sd,
			      struct v4l2_subdev_fh *fh)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_back_subdev_registered(struct v4l2_subdev *sd)
{
	printk(KERN_DEBUG  "%s\n", __func__);
	return 0;
}

static void fimc_is_back_subdev_unregistered(struct v4l2_subdev *sd)
{
	printk(KERN_DEBUG  "%s\n", __func__);
}

static const struct v4l2_subdev_internal_ops fimc_is_sensor_v4l2_internal_ops = {
	.open = fimc_is_sensor_init_formats,
	.close = fimc_is_sensor_subdev_close,
	.registered = fimc_is_sensor_subdev_registered,
	.unregistered = fimc_is_sensor_subdev_unregistered,
};

static const struct v4l2_subdev_internal_ops fimc_is_front_v4l2_internal_ops = {
	.open = fimc_is_front_init_formats,
	.close = fimc_is_front_subdev_close,
	.registered = fimc_is_front_subdev_registered,
	.unregistered = fimc_is_front_subdev_unregistered,
};

static const struct v4l2_subdev_internal_ops fimc_is_back_v4l2_internal_ops = {
	.open = fimc_is_back_init_formats,
	.close = fimc_is_back_subdev_close,
	.registered = fimc_is_back_subdev_registered,
	.unregistered = fimc_is_back_subdev_unregistered,
};

static int fimc_is_sensor_link_setup(struct media_entity *entity,
			    const struct media_pad *local,
			    const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct fimc_is_sensor_dev *fimc_is_sensor = to_fimc_is_sensor_dev(sd);

	printk(KERN_DEBUG "++%s\n", __func__);
	printk(KERN_DEBUG "local->index : %d\n", local->index);
	printk(KERN_DEBUG "media_entity_type(remote->entity) : %d\n", media_entity_type(remote->entity));

	switch (local->index | media_entity_type(remote->entity)) {
	case FIMC_IS_SENSOR_PAD_SOURCE_FRONT | MEDIA_ENT_T_V4L2_SUBDEV:
		if (flags & MEDIA_LNK_FL_ENABLED)
			fimc_is_sensor->output = FIMC_IS_SENSOR_OUTPUT_FRONT;
		else
			fimc_is_sensor->output = FIMC_IS_SENSOR_OUTPUT_NONE;
		break;

	default:
		v4l2_err(sd, "%s : ERR link\n", __func__);
		return -EINVAL;
	}
	printk(KERN_DEBUG "--%s\n", __func__);
	return 0;
}

static int fimc_is_front_link_setup(struct media_entity *entity,
			    const struct media_pad *local,
			    const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct fimc_is_front_dev *fimc_is_front = to_fimc_is_front_dev(sd);

	printk(KERN_DEBUG "++%s\n", __func__);
	printk(KERN_DEBUG "local->index : %d\n", local->index);
	printk(KERN_DEBUG "media_entity_type(remote->entity) : %d\n", media_entity_type(remote->entity));

	switch (local->index | media_entity_type(remote->entity)) {
	case FIMC_IS_FRONT_PAD_SINK | MEDIA_ENT_T_V4L2_SUBDEV:
		printk(KERN_DEBUG "%s : fimc_is_front sink pad\n", __func__);
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (fimc_is_front->input != FIMC_IS_FRONT_INPUT_NONE) {
				printk(KERN_DEBUG "BUSY\n");
				return -EBUSY;
			}
			if (remote->index == FIMC_IS_SENSOR_PAD_SOURCE_FRONT)
				fimc_is_front->input = FIMC_IS_FRONT_INPUT_SENSOR;
		} else {
			fimc_is_front->input = FIMC_IS_FRONT_INPUT_NONE;
		}
		break;

	case FIMC_IS_FRONT_PAD_SOURCE_BACK | MEDIA_ENT_T_V4L2_SUBDEV:
		if (flags & MEDIA_LNK_FL_ENABLED)
			fimc_is_front->output |= FIMC_IS_FRONT_OUTPUT_BACK;
		else
			fimc_is_front->output = FIMC_IS_FRONT_OUTPUT_NONE;
		break;

	case FIMC_IS_FRONT_PAD_SOURCE_BAYER| MEDIA_ENT_T_DEVNODE:
		if (flags & MEDIA_LNK_FL_ENABLED)
			fimc_is_front->output |= FIMC_IS_FRONT_OUTPUT_BAYER;
		else
			fimc_is_front->output = FIMC_IS_FRONT_OUTPUT_NONE;
		break;
	case FIMC_IS_FRONT_PAD_SOURCE_SCALERC| MEDIA_ENT_T_DEVNODE:
		if (flags & MEDIA_LNK_FL_ENABLED)
			fimc_is_front->output |= FIMC_IS_FRONT_OUTPUT_SCALERC;
		else
			fimc_is_front->output = FIMC_IS_FRONT_OUTPUT_NONE;
		break;

	default:
		v4l2_err(sd, "%s : ERR link\n", __func__);
		return -EINVAL;
	}
	printk(KERN_DEBUG "--%s\n", __func__);
	return 0;
}

static int fimc_is_back_link_setup(struct media_entity *entity,
			    const struct media_pad *local,
			    const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct fimc_is_back_dev *fimc_is_back = to_fimc_is_back_dev(sd);

	printk(KERN_DEBUG "++%s\n", __func__);
	switch (local->index | media_entity_type(remote->entity)) {
	case FIMC_IS_BACK_PAD_SINK | MEDIA_ENT_T_V4L2_SUBDEV:
		printk(KERN_DEBUG "%s : fimc_is_back sink pad\n", __func__);
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (fimc_is_back->input != FIMC_IS_BACK_INPUT_NONE) {
				printk(KERN_DEBUG "BUSY\n");
				return -EBUSY;
			}
			if (remote->index == FIMC_IS_FRONT_PAD_SOURCE_BACK)
				fimc_is_back->input = FIMC_IS_BACK_INPUT_FRONT;
		} else {
			fimc_is_back->input = FIMC_IS_FRONT_INPUT_NONE;
		}
		break;
	case FIMC_IS_BACK_PAD_SOURCE_3DNR | MEDIA_ENT_T_DEVNODE:
		if (flags & MEDIA_LNK_FL_ENABLED)
			fimc_is_back->output |= FIMC_IS_BACK_OUTPUT_3DNR;
		else
			fimc_is_back->output = FIMC_IS_FRONT_OUTPUT_NONE;
		break;
	case FIMC_IS_BACK_PAD_SOURCE_SCALERP | MEDIA_ENT_T_DEVNODE:
		if (flags & MEDIA_LNK_FL_ENABLED)
			fimc_is_back->output |= FIMC_IS_BACK_OUTPUT_SCALERP;
		else
			fimc_is_back->output = FIMC_IS_FRONT_OUTPUT_NONE;
		break;
	default:
		v4l2_err(sd, "%s : ERR link\n", __func__);
		return -EINVAL;
	}
	printk(KERN_DEBUG "--%s\n", __func__);
	return 0;
}

static const struct media_entity_operations fimc_is_sensor_media_ops = {
	.link_setup = fimc_is_sensor_link_setup,
};

static const struct media_entity_operations fimc_is_front_media_ops = {
	.link_setup = fimc_is_front_link_setup,
};

static const struct media_entity_operations fimc_is_back_media_ops = {
	.link_setup = fimc_is_back_link_setup,
};

int fimc_is_pipeline_s_stream_preview(struct media_entity *start_entity, int on)
{
	struct media_pad *pad = &start_entity->pads[0];
	struct v4l2_subdev *back_sd;
	struct v4l2_subdev *front_sd;
	struct v4l2_subdev *sensor_sd;
	int	ret;

	printk(KERN_DEBUG "--%s\n", __func__);

	pad = media_entity_remote_source(pad);
	if (media_entity_type(pad->entity) != MEDIA_ENT_T_V4L2_SUBDEV
			|| pad == NULL)
		printk(KERN_DEBUG "cannot find back entity\n");

	back_sd = media_entity_to_v4l2_subdev(pad->entity);

	pad = &pad->entity->pads[0];

	pad = media_entity_remote_source(pad);
	if (media_entity_type(pad->entity) != MEDIA_ENT_T_V4L2_SUBDEV
			|| pad == NULL)
		printk(KERN_DEBUG "cannot find front entity\n");

	front_sd = media_entity_to_v4l2_subdev(pad->entity);

	pad = &pad->entity->pads[0];

	pad = media_entity_remote_source(pad);
	if (media_entity_type(pad->entity) != MEDIA_ENT_T_V4L2_SUBDEV
			|| pad == NULL)
		printk(KERN_DEBUG "cannot find sensor entity\n");

	sensor_sd = media_entity_to_v4l2_subdev(pad->entity);

	if (on) {

		ret = v4l2_subdev_call(sensor_sd, video, s_stream, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;

		ret = v4l2_subdev_call(front_sd, video, s_stream, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;

		ret = v4l2_subdev_call(back_sd, video, s_stream, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;

	} else {
		ret = v4l2_subdev_call(back_sd, video, s_stream, 0);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;
		ret = v4l2_subdev_call(front_sd, video, s_stream, 0);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;
		ret = v4l2_subdev_call(sensor_sd, video, s_stream, 0);
	}

	return ret == -ENOIOCTLCMD ? 0 : ret;
}


static int fimc_is_suspend(struct device *dev)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_resume(struct device *dev)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_runtime_suspend(struct device *dev)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_runtime_resume(struct device *dev)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static int fimc_is_get_md_callback(struct device *dev, void *p)
{
	struct exynos_md **md_list = p;
	struct exynos_md *md = NULL;

	md = dev_get_drvdata(dev);

	if (md)
		*(md_list + md->id) = md;

	return 0; /* non-zero value stops iteration */
}

static struct exynos_md *fimc_is_get_md(enum mdev_node node)
{
	struct device_driver *drv;
	struct exynos_md *md[MDEV_MAX_NUM] = {NULL,};
	int ret;

	drv = driver_find(MDEV_MODULE_NAME, &platform_bus_type);
	if (!drv)
		return ERR_PTR(-ENODEV);

	ret = driver_for_each_device(drv, NULL, &md[0],
				     fimc_is_get_md_callback);
	put_driver(drv);

	return ret ? NULL : md[node];

}

static unsigned int fimc_is_get_intr_position(unsigned int intr_status)
{
	int i;

	for(i=0; i < 5; i++)
		if(intr_status &(1<<i))
			return i;

	return 0;
}

static irqreturn_t fimc_is_irq_handler(int irq, void *dev_id)
{
	struct fimc_is_dev *dev = dev_id;
	int buf_index;
	unsigned int intr_status, intr_pos;

	intr_status = readl(dev->regs + INTSR1);
	intr_pos = fimc_is_get_intr_position(intr_status);

	if(intr_pos == INTR_GENERAL){
		dev->i2h_cmd.cmd = readl(dev->regs + ISSR10);

		/* Read ISSR10 ~ ISSR15 */
		switch (dev->i2h_cmd.cmd) {
		case IHC_GET_SENSOR_NUMBER:
			dbg("IHC_GET_SENSOR_NUMBER\n");
			fimc_is_hw_get_param(dev, 1);
			dbg("ISP - FW version - %d\n", dev->i2h_cmd.arg[0]);
			dev->fw.ver = dev->i2h_cmd.arg[0];
			fimc_is_hw_wait_intmsr0_intmsd0(dev);
			fimc_is_hw_set_sensor_num(dev);
			break;
		case IHC_LOAD_SET_FILE:
			fimc_is_hw_get_param(dev, 2);
		case IHC_SET_SHOT_MARK:
			fimc_is_hw_get_param(dev, 3);
			break;
		case IHC_SET_FACE_MARK:
			fimc_is_hw_get_param(dev, 2);
			break;
		case IHC_NOT_READY:
			break;
		case IHC_AA_DONE:
			fimc_is_hw_get_param(dev, 3);
			break;
		case ISR_DONE:
			fimc_is_hw_get_param(dev, 3);
			break;
		case ISR_NDONE:
			fimc_is_hw_get_param(dev, 4);
			/* fimc_is_fw_clear_insr1(dev); */
			break;
		}
		/* Just clear the interrupt pending bits. */
		fimc_is_fw_clear_irq1(dev, intr_pos);

		switch (dev->i2h_cmd.cmd) {
		case IHC_GET_SENSOR_NUMBER:
			fimc_is_hw_set_intgr0_gd0(dev);
			set_bit(IS_ST_FW_DOWNLOADED, &dev->state);
			break;
		case IHC_LOAD_SET_FILE:
			dev->setfile.base = dev->i2h_cmd.arg[0];
			dev->setfile.size = dev->i2h_cmd.arg[1];
			set_bit(IS_ST_SET_FILE, &dev->state);
			dbg("IHC_LOAD_SET_FILE\n");
			break;
		case IHC_SET_SHOT_MARK:
			break;
		case IHC_SET_FACE_MARK:
			dev->fd_header.count = dev->i2h_cmd.arg[0];
			dev->fd_header.index = dev->i2h_cmd.arg[1];
			break;
		case IHC_AA_DONE:
			dbg("AA_DONE - %d, %d, %d\n", dev->i2h_cmd.arg[0],
				dev->i2h_cmd.arg[1], dev->i2h_cmd.arg[2]);
			if (dev->af.af_state == FIMC_IS_AF_RUNNING)
				dev->af.af_state = FIMC_IS_AF_LOCK;
			dev->af.af_lock_state = dev->i2h_cmd.arg[0];
			dev->af.ae_lock_state = dev->i2h_cmd.arg[1];
			dev->af.awb_lock_state = dev->i2h_cmd.arg[2];
			break;
		case IHC_NOT_READY:
			err("Init Sequnce Error- IS will be turned off!!");
			break;
		case ISR_DONE:
			dbg("ISR_DONE - %d\n", dev->i2h_cmd.arg[0]);
			switch (dev->i2h_cmd.arg[0]) {
			case HIC_PREVIEW_STILL:
			case HIC_PREVIEW_VIDEO:
			case HIC_CAPTURE_STILL:
			case HIC_CAPTURE_VIDEO:
				if (test_and_clear_bit(IS_ST_CHANGE_MODE,
					&dev->state)) {
					dev->sensor.offset_x = dev->i2h_cmd.arg[1];
					dev->sensor.offset_y = dev->i2h_cmd.arg[2];
					set_bit(IS_ST_CHANGE_MODE_DONE, &dev->state);
				}
				break;
			case HIC_STREAM_ON:
				clear_bit(IS_ST_CHANGE_MODE_DONE, &dev->state);
				set_bit(IS_ST_STREAM_ON, &dev->state);
				break;
			case HIC_STREAM_OFF:
				set_bit(IS_ST_STREAM_OFF, &dev->state);
				set_bit(IS_ST_RUN, &dev->state);
				break;
			case HIC_SET_PARAMETER:
				dev->p_region_index1 = 0;
				dev->p_region_index2 = 0;
				atomic_set(&dev->p_region_num, 0);
				if (test_bit(IS_ST_INIT_CAPTURE_VIDEO, &dev->state))
					set_bit(IS_ST_RUN, &dev->state);
				else if (test_bit(IS_ST_INIT_CAPTURE_STILL,
					&dev->state))
					set_bit(IS_ST_INIT_CAPTURE_VIDEO, &dev->state);
				else if (test_bit(IS_ST_INIT_PREVIEW_VIDEO,
					&dev->state))
					set_bit(IS_ST_INIT_CAPTURE_STILL, &dev->state);
				else if (test_bit(IS_ST_INIT_PREVIEW_STILL,
					&dev->state))
					set_bit(IS_ST_INIT_PREVIEW_VIDEO, &dev->state);
				else {
					clear_bit(IS_ST_SET_PARAM, &dev->state);
					set_bit(IS_ST_RUN, &dev->state);
				}

				if (dev->af.af_state == FIMC_IS_AF_SETCONFIG)
					dev->af.af_state = FIMC_IS_AF_RUNNING;
				else if (dev->af.af_state == FIMC_IS_AF_ABORT)
					dev->af.af_state = FIMC_IS_AF_IDLE;
				break;
			case HIC_GET_PARAMETER:
				break;
			case HIC_SET_TUNE:
				break;
			case HIC_GET_STATUS:
				break;
			case HIC_OPEN_SENSOR:
				set_bit(IS_ST_INIT_PREVIEW_STILL, &dev->state);
				dbg("reply HIC_OPEN_SENSOR");
				break;
			case HIC_CLOSE_SENSOR:
				set_bit(IS_ST_INIT_CAPTURE_VIDEO, &dev->state);
				dev->sensor.id = 0;
				break;
			case HIC_POWER_DOWN:
				clear_bit(FIMC_IS_PWR_ST_POWERED, &dev->power);
				set_bit(FIMC_IS_PWR_ST_POWEROFF, &dev->power);
				break;
			}
			break;
		case ISR_NDONE:
			err("ISR_NDONE - %d: %d\n", dev->i2h_cmd.arg[0],
				dev->i2h_cmd.arg[1]);

			switch (dev->i2h_cmd.arg[1]) {
			case IS_ERROR_SET_PARAMETER:
				fimc_is_mem_cache_inv((void *)dev->is_p_region,
					IS_PARAM_SIZE);
				break;
			}

			break;
		}
	}
	else if(intr_pos == INTR_FRAME_DONE_SCALERC){
		dev->i2h_cmd.arg[0] = readl(dev->regs + ISSR24);
		dev->i2h_cmd.arg[1] = readl(dev->regs + ISSR25);
		fimc_is_fw_clear_irq1(dev, intr_pos);

		buf_index = (dev->i2h_cmd.arg[1] - 1)
					% dev->video[FIMC_IS_VIDEO_NUM_SCALERC].num_buf;
		vb2_buffer_done(dev->video[FIMC_IS_VIDEO_NUM_SCALERC].vbq.bufs[buf_index],
						VB2_BUF_STATE_DONE);
	}
	else if(intr_pos == INTR_FRAME_DONE_TDNR){
		dev->i2h_cmd.arg[0] = readl(dev->regs + ISSR28);
		dev->i2h_cmd.arg[1] = readl(dev->regs + ISSR29);
		fimc_is_fw_clear_irq1(dev, intr_pos);

		buf_index = (dev->i2h_cmd.arg[1] - 1)
					% dev->video[FIMC_IS_VIDEO_NUM_3DNR].num_buf;
		vb2_buffer_done(dev->video[FIMC_IS_VIDEO_NUM_3DNR].vbq.bufs[buf_index],
						VB2_BUF_STATE_DONE);

	}
	else if(intr_pos == INTR_FRAME_DONE_SCALERP){
		dev->i2h_cmd.arg[0] = readl(dev->regs + ISSR32);
		dev->i2h_cmd.arg[1] = readl(dev->regs + ISSR33);
		fimc_is_fw_clear_irq1(dev, intr_pos);

		buf_index = (dev->i2h_cmd.arg[1] - 1)
					% dev->video[FIMC_IS_VIDEO_NUM_SCALERP].num_buf;
		vb2_buffer_done(dev->video[FIMC_IS_VIDEO_NUM_SCALERP].vbq.bufs[buf_index],
						VB2_BUF_STATE_DONE);


	}
	wake_up(&dev->irq_queue);

	return IRQ_HANDLED;
}

static int fimc_is_probe(struct platform_device *pdev)
{
	struct resource *mem_res;
	struct resource *regs_res;
	struct fimc_is_dev *isp;
	int ret = -ENODEV;
	struct vb2_queue *scalerc_q;
	struct vb2_queue *scalerp_q;
	struct vb2_queue *dnr_q;

	printk(KERN_DEBUG "fimc_is_front_probe\n");

	isp = kzalloc(sizeof(struct fimc_is_dev), GFP_KERNEL);
	if (!isp)
		return -ENOMEM;

	isp->pdev = pdev;
	isp->pdata = pdev->dev.platform_data;
	isp->id = pdev->id;
	isp->pipe_state = 0;

	set_bit(FIMC_IS_STATE_IDLE, &isp->pipe_state);
	set_bit(IS_ST_IDLE , &isp->state);

	init_waitqueue_head(&isp->irq_queue);
	spin_lock_init(&isp->slock);
	mutex_init(&isp->lock);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		dev_err(&pdev->dev, "Failed to get io memory region\n");
		goto p_err1;
	}

	regs_res = request_mem_region(mem_res->start, resource_size(mem_res),
				      pdev->name);
	if (!regs_res) {
		dev_err(&pdev->dev, "Failed to request io memory region\n");
		goto p_err1;
	}

	isp->regs_res = regs_res;
	isp->regs = ioremap(mem_res->start, resource_size(mem_res));
	if (!isp->regs) {
		dev_err(&pdev->dev, "Failed to remap io region\n");
		goto p_err2;
	}

	isp->mdev = fimc_is_get_md(MDEV_ISP);
	if (IS_ERR_OR_NULL(isp->mdev))
		goto p_err3;

	printk(KERN_DEBUG "fimc_is_front->mdev : 0x%p\n", isp->mdev);

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
	isp->vb2 = &fimc_is_vb2_cma;
#elif defined(CONFIG_VIDEOBUF2_ION)
	isp->vb2 = &fimc_is_vb2_ion;
#endif

	isp->irq = platform_get_irq(pdev, 0);
	if (isp->irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq\n");
		goto p_err3;
	}

	ret = request_irq(isp->irq, fimc_is_irq_handler, 0, dev_name(&pdev->dev), isp);
	if (ret) {
		dev_err(&pdev->dev, "request_irq failed\n");
		goto p_err3;
	}

	/*sensor entity*/
	v4l2_subdev_init(&isp->sensor.sd, &fimc_is_sensor_subdev_ops);
	isp->sensor.sd.owner = THIS_MODULE;
	isp->sensor.sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(isp->sensor.sd.name, sizeof(isp->sensor.sd.name), "%s\n",
					FIMC_IS_SENSOR_ENTITY_NAME);

	isp->sensor.pads.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&isp->sensor.sd.entity, 1,
				&isp->sensor.pads, 0);
	if (ret < 0)
		goto p_err3;

	fimc_is_sensor_init_formats(&isp->sensor.sd, NULL);

	isp->sensor.sd.internal_ops = &fimc_is_sensor_v4l2_internal_ops;
	isp->sensor.sd.entity.ops = &fimc_is_sensor_media_ops;

	ret = v4l2_device_register_subdev(&isp->mdev->v4l2_dev, &isp->sensor.sd);
	if (ret)
		goto p_err3;
	/* This allows to retrieve the platform device id by the host driver */
	v4l2_set_subdevdata(&isp->sensor.sd, pdev);


	/*front entity*/
	v4l2_subdev_init(&isp->front.sd, &fimc_is_front_subdev_ops);
	isp->front.sd.owner = THIS_MODULE;
	isp->front.sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(isp->front.sd.name, sizeof(isp->front.sd.name), "%s\n",
					FIMC_IS_FRONT_ENTITY_NAME);

	isp->front.pads[FIMC_IS_FRONT_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	isp->front.pads[FIMC_IS_FRONT_PAD_SOURCE_BACK].flags = MEDIA_PAD_FL_SOURCE;
	isp->front.pads[FIMC_IS_FRONT_PAD_SOURCE_BAYER].flags = MEDIA_PAD_FL_SOURCE;
	isp->front.pads[FIMC_IS_FRONT_PAD_SOURCE_SCALERC].flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&isp->front.sd.entity, FIMC_IS_FRONT_PADS_NUM,
				isp->front.pads, 0);
	if (ret < 0)
		goto p_err3;

	fimc_is_front_init_formats(&isp->front.sd, NULL);

	isp->front.sd.internal_ops = &fimc_is_front_v4l2_internal_ops;
	isp->front.sd.entity.ops = &fimc_is_front_media_ops;

	ret = v4l2_device_register_subdev(&isp->mdev->v4l2_dev, &isp->front.sd);
	if (ret)
		goto p_err3;
	/* This allows to retrieve the platform device id by the host driver */
	v4l2_set_subdevdata(&isp->front.sd, pdev);


	/*back entity*/
	v4l2_subdev_init(&isp->back.sd, &fimc_is_back_subdev_ops);
	isp->back.sd.owner = THIS_MODULE;
	isp->back.sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(isp->back.sd.name, sizeof(isp->back.sd.name), "%s\n",
					FIMC_IS_BACK_ENTITY_NAME);

	isp->back.pads[FIMC_IS_BACK_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	isp->back.pads[FIMC_IS_BACK_PAD_SOURCE_3DNR].flags = MEDIA_PAD_FL_SOURCE;
	isp->back.pads[FIMC_IS_BACK_PAD_SOURCE_SCALERP].flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&isp->back.sd.entity, FIMC_IS_BACK_PADS_NUM,
				isp->back.pads, 0);
	if (ret < 0)
		goto p_err3;

	fimc_is_front_init_formats(&isp->back.sd, NULL);

	isp->back.sd.internal_ops = &fimc_is_back_v4l2_internal_ops;
	isp->back.sd.entity.ops = &fimc_is_back_media_ops;

	ret = v4l2_device_register_subdev(&isp->mdev->v4l2_dev, &isp->back.sd);
	if (ret)
		goto p_err3;

	v4l2_set_subdevdata(&isp->back.sd, pdev);

	/*front video entity - scalerC */
	snprintf(isp->video[FIMC_IS_VIDEO_NUM_SCALERC].vd.name, sizeof(isp->video[FIMC_IS_VIDEO_NUM_SCALERC].vd.name),
			"%s", FIMC_IS_VIDEO_SCALERC_NAME);

	isp->video[FIMC_IS_VIDEO_NUM_SCALERC].vd.fops	= &fimc_is_scalerc_video_fops;
	isp->video[FIMC_IS_VIDEO_NUM_SCALERC].vd.ioctl_ops	= &fimc_is_scalerc_video_ioctl_ops;
	isp->video[FIMC_IS_VIDEO_NUM_SCALERC].vd.v4l2_dev	= &isp->mdev->v4l2_dev;
	isp->video[FIMC_IS_VIDEO_NUM_SCALERC].vd.minor	= -1;
	isp->video[FIMC_IS_VIDEO_NUM_SCALERC].vd.release	= video_device_release;
	isp->video[FIMC_IS_VIDEO_NUM_SCALERC].vd.lock	= &isp->lock;
	video_set_drvdata(&isp->video[FIMC_IS_VIDEO_NUM_SCALERC].vd, isp);
	isp->video[FIMC_IS_VIDEO_NUM_SCALERC].dev = isp;

	scalerc_q = &isp->video[FIMC_IS_VIDEO_NUM_SCALERC].vbq;
	memset(scalerc_q, 0, sizeof(*scalerc_q));
	scalerc_q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	scalerc_q->io_modes = VB2_MMAP | VB2_USERPTR;
	scalerc_q->drv_priv = &isp->video[FIMC_IS_VIDEO_NUM_SCALERC];
	scalerc_q->ops = &fimc_is_scalerc_qops;
	scalerc_q->mem_ops = isp->vb2->ops;;

	vb2_queue_init(scalerc_q);

	ret = video_register_device(&isp->video[FIMC_IS_VIDEO_NUM_SCALERC].vd, VFL_TYPE_GRABBER,
					FIMC_IS_VIDEO_NUM_SCALERC+EXYNOS_VIDEONODE_FIMC_IS);
	printk(KERN_DEBUG "VIDEO NODE :: scalerC minor : %d\n", isp->video[FIMC_IS_VIDEO_NUM_SCALERC].vd.minor);
	if (ret) {
		printk(KERN_ERR "%s : Failed to register ScalerC video device\n", __func__);
		goto p_err3;
	}
	isp->video[FIMC_IS_VIDEO_NUM_SCALERC].pads.flags = MEDIA_PAD_FL_SINK;
	ret = media_entity_init(&isp->video[FIMC_IS_VIDEO_NUM_SCALERC].vd.entity, 1, &isp->video[FIMC_IS_VIDEO_NUM_SCALERC].pads, 0);
	if (ret) {
		printk(KERN_ERR "%s : Failed to media_entity_init ScalerC video device\n", __func__);
		goto p_err3;
	}

	/* back video entity - scalerP*/
	snprintf(isp->video[FIMC_IS_VIDEO_NUM_SCALERP].vd.name, sizeof(isp->video[FIMC_IS_VIDEO_NUM_SCALERP].vd.name),
			"%s", FIMC_IS_VIDEO_SCALERP_NAME);

	isp->video[FIMC_IS_VIDEO_NUM_SCALERP].vd.fops	= &fimc_is_scalerp_video_fops;
	isp->video[FIMC_IS_VIDEO_NUM_SCALERP].vd.ioctl_ops	= &fimc_is_scalerp_video_ioctl_ops;
	isp->video[FIMC_IS_VIDEO_NUM_SCALERP].vd.v4l2_dev	= &isp->mdev->v4l2_dev;
	isp->video[FIMC_IS_VIDEO_NUM_SCALERP].vd.minor	= -1;
	isp->video[FIMC_IS_VIDEO_NUM_SCALERP].vd.release	= video_device_release;
	isp->video[FIMC_IS_VIDEO_NUM_SCALERP].vd.lock	= &isp->lock;
	video_set_drvdata(&isp->video[FIMC_IS_VIDEO_NUM_SCALERP].vd, isp);
	isp->video[FIMC_IS_VIDEO_NUM_SCALERP].dev = isp;

	scalerp_q = &isp->video[FIMC_IS_VIDEO_NUM_SCALERP].vbq;
	memset(scalerp_q, 0, sizeof(*scalerp_q));
	scalerp_q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	scalerp_q->io_modes = VB2_MMAP | VB2_USERPTR;
	scalerp_q->drv_priv = &isp->video[FIMC_IS_VIDEO_NUM_SCALERP];
	scalerp_q->ops = &fimc_is_scalerp_qops;
	scalerp_q->mem_ops = isp->vb2->ops;;

	vb2_queue_init(scalerp_q);

	ret = video_register_device(&isp->video[FIMC_IS_VIDEO_NUM_SCALERP].vd, VFL_TYPE_GRABBER,
					FIMC_IS_VIDEO_NUM_SCALERP+EXYNOS_VIDEONODE_FIMC_IS);
	printk(KERN_DEBUG "VIDEO NODE :: scalerP minor : %d\n", isp->video[FIMC_IS_VIDEO_NUM_SCALERP].vd.minor);
	if (ret) {
		printk(KERN_ERR "%s : Failed to register ScalerP video device\n", __func__);
		goto p_err3;
	}
	isp->video[FIMC_IS_VIDEO_NUM_SCALERP].pads.flags = MEDIA_PAD_FL_SINK;
	ret = media_entity_init(&isp->video[FIMC_IS_VIDEO_NUM_SCALERP].vd.entity, 1, &isp->video[FIMC_IS_VIDEO_NUM_SCALERP].pads, 0);
	if (ret) {
		printk(KERN_ERR "%s : Failed to media_entity_init ScalerP video device\n", __func__);
		goto p_err3;
	}

	/*back video entity - 3DNR */
	snprintf(isp->video[FIMC_IS_VIDEO_NUM_3DNR].vd.name, sizeof(isp->video[FIMC_IS_VIDEO_NUM_3DNR].vd.name),
			"%s", FIMC_IS_VIDEO_3DNR_NAME);

	isp->video[FIMC_IS_VIDEO_NUM_3DNR].vd.fops	= &fimc_is_3dnr_video_fops;
	isp->video[FIMC_IS_VIDEO_NUM_3DNR].vd.ioctl_ops	= &fimc_is_3dnr_video_ioctl_ops;
	isp->video[FIMC_IS_VIDEO_NUM_3DNR].vd.v4l2_dev	= &isp->mdev->v4l2_dev;
	isp->video[FIMC_IS_VIDEO_NUM_3DNR].vd.minor	= -1;
	isp->video[FIMC_IS_VIDEO_NUM_3DNR].vd.release	= video_device_release;
	isp->video[FIMC_IS_VIDEO_NUM_3DNR].vd.lock	= &isp->lock;
	video_set_drvdata(&isp->video[FIMC_IS_VIDEO_NUM_3DNR].vd, isp);
	isp->video[FIMC_IS_VIDEO_NUM_3DNR].dev = isp;

	dnr_q = &isp->video[FIMC_IS_VIDEO_NUM_3DNR].vbq;
	memset(dnr_q, 0, sizeof(*dnr_q));
	dnr_q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dnr_q->io_modes = VB2_MMAP | VB2_USERPTR;
	dnr_q->drv_priv = &isp->video[FIMC_IS_VIDEO_NUM_3DNR];
	dnr_q->ops = &fimc_is_3dnr_qops;
	dnr_q->mem_ops = isp->vb2->ops;;

	vb2_queue_init(dnr_q);

	ret = video_register_device(&isp->video[FIMC_IS_VIDEO_NUM_3DNR].vd, VFL_TYPE_GRABBER,
					FIMC_IS_VIDEO_NUM_3DNR+EXYNOS_VIDEONODE_FIMC_IS);
	printk(KERN_DEBUG "VIDEO NODE :: 3DNR minor : %d\n", isp->video[FIMC_IS_VIDEO_NUM_3DNR].vd.minor);
	if (ret) {
		printk(KERN_ERR "%s : Failed to register 3DNR video device\n", __func__);
		goto p_err3;
	}
	isp->video[FIMC_IS_VIDEO_NUM_3DNR].pads.flags = MEDIA_PAD_FL_SINK;
	ret = media_entity_init(&isp->video[FIMC_IS_VIDEO_NUM_3DNR].vd.entity, 1, &isp->video[FIMC_IS_VIDEO_NUM_3DNR].pads, 0);
	if (ret) {
		printk(KERN_ERR "%s : Failed to media_entity_init 3DNR video device\n", __func__);
		goto p_err3;
	}

	platform_set_drvdata(pdev, &isp);

	/* create link */
	ret = media_entity_create_link(
			&isp->sensor.sd.entity, FIMC_IS_SENSOR_PAD_SOURCE_FRONT,
			&isp->front.sd.entity, FIMC_IS_FRONT_PAD_SINK, 0);
	if (ret < 0) {
		printk(KERN_ERR "failed link creation from sensor to front\n");
		goto p_err3;
	}

	ret = media_entity_create_link(
			&isp->front.sd.entity, FIMC_IS_FRONT_PAD_SOURCE_BACK,
			&isp->back.sd.entity, FIMC_IS_BACK_PAD_SINK, 0);
	if (ret < 0) {
		printk(KERN_ERR "failed link creation from front to back\n");
		goto p_err3;
	}

	ret = media_entity_create_link(
			&isp->front.sd.entity, FIMC_IS_FRONT_PAD_SOURCE_SCALERC,
			&isp->video[FIMC_IS_VIDEO_NUM_SCALERC].vd.entity, 0, 0);

	if (ret < 0) {
		printk(KERN_ERR "failed link creation from front to scalerC video\n");
		goto p_err3;
	}

	ret = media_entity_create_link(
			&isp->back.sd.entity, FIMC_IS_BACK_PAD_SOURCE_SCALERP,
			&isp->video[FIMC_IS_VIDEO_NUM_SCALERP].vd.entity, 0, 0);

	if (ret < 0) {
		printk(KERN_ERR "failed link creation from back to scalerP video\n");
		goto p_err3;
	}

	ret = media_entity_create_link(
			&isp->back.sd.entity, FIMC_IS_BACK_PAD_SOURCE_3DNR,
			&isp->video[FIMC_IS_VIDEO_NUM_3DNR].vd.entity, 0, 0);

	if (ret < 0) {
		printk(KERN_ERR "failed link creation from back to 3DNR video\n");
		goto p_err3;
	}

	/* register subdev nodes*/
	ret = v4l2_device_register_subdev_nodes(&isp->mdev->v4l2_dev);
	if (ret)
		printk(KERN_ERR "v4l2_device_register_subdev_nodes failed\n");

	/* init vb2*/
	isp->alloc_ctx = isp->vb2->init(isp);
	if (IS_ERR(isp->alloc_ctx)) {
		ret = PTR_ERR(isp->alloc_ctx);
		goto p_err1;
	}

	/* init memory*/
	ret = fimc_is_init_mem(isp);
	if (ret) {
		printk(
			"failed to fimc_is_init_mem (%d)\n", ret);
		goto p_err3;
	}

	/*init gpio : should be moved to stream_on */
	if (isp->pdata->cfg_gpio) {
		isp->pdata->cfg_gpio(isp->pdev);
	} else {
		dev_err(&isp->pdev->dev, "failed to init GPIO config\n");
		goto p_err3;
	}

#if defined(CONFIG_VIDEOBUF2_ION)
	if (isp->alloc_ctx)
		fimc_is_mem_resume(isp->alloc_ctx);
#endif
	printk(KERN_DEBUG "%s : fimc_is_front_%d probe success\n", __func__, pdev->id);
	printk("probe2\n");
	return 0;

p_err3:
	iounmap(isp->regs);
p_err2:
	release_mem_region(regs_res->start, resource_size(regs_res));
p_err1:
	kfree(isp);
	return ret;
}

static int fimc_is_remove(struct platform_device *pdev)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops fimc_is_pm_ops = {
	.suspend		= fimc_is_suspend,
	.resume			= fimc_is_resume,
	.runtime_suspend	= fimc_is_runtime_suspend,
	.runtime_resume		= fimc_is_runtime_resume,
};

static struct platform_driver fimc_is_driver = {
	.probe		= fimc_is_probe,
	.remove	= __devexit_p(fimc_is_remove),
	.driver = {
		.name	= FIMC_IS_MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_pm_ops,
	}
};

static int __init fimc_is_init(void)
{
	int ret = platform_driver_register(&fimc_is_driver);
	if (ret)
		err("platform_driver_register failed: %d\n", ret);
	return ret;
}

static void __exit fimc_is_exit(void)
{
	platform_driver_unregister(&fimc_is_driver);
}
module_init(fimc_is_init);
module_exit(fimc_is_exit);

MODULE_AUTHOR("Jiyoung Shin<idon.shin@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC_IS front end driver");
MODULE_LICENSE("GPL");
