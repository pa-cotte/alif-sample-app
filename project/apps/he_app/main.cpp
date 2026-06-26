/*
 * Copyright (c) 2016 Intel Corporation.
 * Copyright (c) 2019-2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * USB Mass Storage sample ported to the Alif E7 DK (RTSS-HE).
 * Exposes a RAM-backed FAT disk over USB using the new (next) USB device
 * stack and the Synopsys DWC3 UDC controller.
 *
 * Based on zephyr/samples/subsys/usb/mass.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usbd_msg.h>
#include <zephyr/usb/class/usbd_msc.h>
#include <zephyr/fs/fs.h>
#include <stdio.h>

#if defined(CONFIG_SHELL)
#include <zephyr/shell/shell.h>
#endif

/*
 * sample_usbd.h (samples/subsys/usb/common) has no extern "C" guard and pulls
 * in C++-incompatible templated headers, so we declare the helpers it provides
 * directly with C linkage instead of including it. They are implemented in the
 * C-compiled sample_usbd_init.c.
 */
extern "C" {
struct usbd_context *sample_usbd_init_device(usbd_msg_cb_t msg_cb);
struct usbd_context *sample_usbd_setup_device(usbd_msg_cb_t msg_cb);
}

LOG_MODULE_REGISTER(main);

#if CONFIG_DISK_DRIVER_FLASH
#include <zephyr/storage/flash_map.h>
#endif

#if CONFIG_FAT_FILESYSTEM_ELM
#include <ff.h>
#endif

#if CONFIG_FILE_SYSTEM_LITTLEFS
#include <zephyr/fs/littlefs.h>
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
#endif

#if !defined(CONFIG_DISK_DRIVER_FLASH) && \
	!defined(CONFIG_DISK_DRIVER_RAM) && \
	!defined(CONFIG_DISK_DRIVER_SDMMC)
#error No supported disk driver enabled
#endif

#define STORAGE_PARTITION		ospi_storage
#define STORAGE_PARTITION_ID		FIXED_PARTITION_ID(STORAGE_PARTITION)

static struct fs_mount_t fs_mnt;
static bool fs_mounted;

static int mount_app_fs(struct fs_mount_t *mnt);

/*
 * The USB host accesses the disk at the block level, bypassing the device-side
 * file system. Two file-system owners on the same OSPI flash break host access
 * and risk FAT corruption, so the device keeps its FATFS unmounted while the
 * host is connected and mounts it only while the host is away.
 */
static int mount_fs(void)
{
	int rc;

	if (fs_mounted) {
		return 0;
	}

	rc = mount_app_fs(&fs_mnt);
	if (rc == 0) {
		fs_mounted = true;
	}

	return rc;
}

static int unmount_fs(void)
{
	int rc;

	if (!fs_mounted) {
		return 0;
	}

	rc = fs_unmount(&fs_mnt);
	if (rc == 0) {
		fs_mounted = false;
	}

	return rc;
}

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
static struct usbd_context *sample_usbd;

/*
 * The FS refresh runs on its own work queue, not the system one: remounting the
 * OSPI flash uses a deep call stack that overflows the small sysworkq stack.
 */
#define FS_REFRESH_STACK_SIZE	8192
#define FS_REFRESH_PRIORITY	5
static K_THREAD_STACK_DEFINE(fs_refresh_stack, FS_REFRESH_STACK_SIZE);
static struct k_work_q fs_refresh_q;

/* 1 while the USB host owns the disk (keep device FATFS unmounted), 0 otherwise. */
static atomic_t fs_host_owns_disk;

/* Reconciles the mount state to whoever owns the disk, off the workqueue. */
static void fs_sync_work_handler(struct k_work *work)
{
	int rc;

	if (atomic_get(&fs_host_owns_disk)) {
		rc = unmount_fs();
		if (rc < 0) {
			LOG_ERR("Unmount for USB host access failed: %d", rc);
		} else {
			LOG_INF("FS released to USB host");
		}
	} else {
		rc = mount_fs();
		if (rc < 0) {
			LOG_ERR("Mount after USB disconnect failed: %d", rc);
		} else {
			LOG_INF("FS mounted for inspection (%s)", fs_mnt.mnt_point);
		}
	}
}

static K_WORK_DEFINE(fs_sync_work, fs_sync_work_handler);

static void usb_msg_cb(struct usbd_context *const ctx,
		       const struct usbd_msg *const msg)
{
	LOG_DBG("USBD message: %s", usbd_msg_type_string(msg->type));

	/*
	 * Hand the disk to whoever is using it. While the host is connected it
	 * owns the block device, so the device-side FATFS must stay unmounted.
	 * Once the host is unplugged (VBUS removed), mount so host-written files
	 * can be inspected. The actual mount/unmount runs on a dedicated work
	 * queue because the callback runs on the shared system workqueue and an
	 * OSPI mount uses a deep, blocking call stack.
	 */
	switch (msg->type) {
	case USBD_MSG_VBUS_READY:
		atomic_set(&fs_host_owns_disk, 1);
		k_work_submit_to_queue(&fs_refresh_q, &fs_sync_work);
		break;
	case USBD_MSG_VBUS_REMOVED:
		atomic_set(&fs_host_owns_disk, 0);
		k_work_submit_to_queue(&fs_refresh_q, &fs_sync_work);
		break;
	default:
		break;
	}
}

#if CONFIG_DISK_DRIVER_RAM
USBD_DEFINE_MSC_LUN(ram, "RAM", "Zephyr", "RAMDisk", "0.00");
#endif

#if CONFIG_DISK_DRIVER_FLASH
USBD_DEFINE_MSC_LUN(nand, "NAND", "Zephyr", "FlashDisk", "0.00");
#endif

#if CONFIG_DISK_DRIVER_SDMMC
USBD_DEFINE_MSC_LUN(sd, "SD", "Zephyr", "SD", "0.00");
#endif

static int enable_usb_device_next(void)
{
	int err;

	sample_usbd = sample_usbd_init_device(usb_msg_cb);
	if (sample_usbd == NULL) {
		LOG_ERR("Failed to initialize USB device");
		return -ENODEV;
	}

	err = usbd_enable(sample_usbd);
	if (err) {
		LOG_ERR("Failed to enable device support");
		return err;
	}

	LOG_DBG("USB device support enabled");

	return 0;
}
#endif /* defined(CONFIG_USB_DEVICE_STACK_NEXT) */

static int setup_flash(struct fs_mount_t *mnt)
{
	int rc = 0;
#if CONFIG_DISK_DRIVER_FLASH
	unsigned int id;
	const struct flash_area *pfa;

	mnt->storage_dev = (void *)STORAGE_PARTITION_ID;
	id = STORAGE_PARTITION_ID;

	rc = flash_area_open(id, &pfa);
	printk("Area %u at 0x%x on %s for %u bytes\n",
	       id, (unsigned int)pfa->fa_off, pfa->fa_dev->name,
	       (unsigned int)pfa->fa_size);

	if (rc < 0 && IS_ENABLED(CONFIG_APP_WIPE_STORAGE)) {
		printk("Erasing flash area ... ");
		rc = flash_area_flatten(pfa, 0, pfa->fa_size);
		printk("%d\n", rc);
	}

	if (rc < 0) {
		flash_area_close(pfa);
	}
#endif
	return rc;
}

static int mount_app_fs(struct fs_mount_t *mnt)
{
	int rc;

#if CONFIG_FAT_FILESYSTEM_ELM
	static FATFS fat_fs;

	mnt->type = FS_FATFS;
	mnt->fs_data = &fat_fs;
	if (IS_ENABLED(CONFIG_DISK_DRIVER_RAM)) {
		mnt->mnt_point = "/RAM:";
	} else if (IS_ENABLED(CONFIG_DISK_DRIVER_SDMMC)) {
		mnt->mnt_point = "/SD:";
	} else {
		mnt->mnt_point = "/NAND:";
	}

#elif CONFIG_FILE_SYSTEM_LITTLEFS
	mnt->type = FS_LITTLEFS;
	mnt->mnt_point = "/lfs";
	mnt->fs_data = &storage;
#endif
	rc = fs_mount(mnt);

	return rc;
}

static void setup_disk(void)
{
	struct fs_mount_t *mp = &fs_mnt;
	struct fs_dir_t dir;
	struct fs_statvfs sbuf;
	int rc;

	fs_dir_t_init(&dir);

	if (IS_ENABLED(CONFIG_DISK_DRIVER_FLASH)) {
		rc = setup_flash(mp);
		if (rc < 0) {
			LOG_ERR("Failed to setup flash area");
			return;
		}
	}

	if (!IS_ENABLED(CONFIG_FILE_SYSTEM_LITTLEFS) &&
	    !IS_ENABLED(CONFIG_FAT_FILESYSTEM_ELM)) {
		LOG_INF("No file system selected");
		return;
	}

	rc = mount_fs();
	if (rc < 0) {
		LOG_ERR("Failed to mount filesystem");
		return;
	}

	/* Allow log messages to flush to avoid interleaved output */
	k_sleep(K_MSEC(50));

	printk("Mount %s: %d\n", fs_mnt.mnt_point, rc);

	rc = fs_statvfs(mp->mnt_point, &sbuf);
	if (rc < 0) {
		printk("FAIL: statvfs: %d\n", rc);
		return;
	}

	printk("%s: bsize = %lu ; frsize = %lu ;"
	       " blocks = %lu ; bfree = %lu\n",
	       mp->mnt_point,
	       sbuf.f_bsize, sbuf.f_frsize,
	       sbuf.f_blocks, sbuf.f_bfree);

	rc = fs_opendir(&dir, mp->mnt_point);
	printk("%s opendir: %d\n", mp->mnt_point, rc);

	if (rc < 0) {
		LOG_ERR("Failed to open directory");
	}

	while (rc >= 0) {
		struct fs_dirent ent = {};

		rc = fs_readdir(&dir, &ent);
		if (rc < 0) {
			LOG_ERR("Failed to read directory entries");
			break;
		}
		if (ent.name[0] == 0) {
			printk("End of files\n");
			break;
		}
		printk("  %c %u %s\n",
		       (ent.type == FS_DIR_ENTRY_FILE) ? 'F' : 'D',
		       ent.size,
		       ent.name);
	}

	(void)fs_closedir(&dir);

	/*
	 * Leave the disk formatted but unmounted so the USB host gets exclusive
	 * ownership when it connects (see usb_msg_cb).
	 */
	unmount_fs();

	return;
}

#if defined(CONFIG_SHELL)
/*
 * Force a fresh mount for inspection without unplugging the cable. Note that
 * while the USB host is connected it owns the disk, so this temporarily takes
 * ownership on the device side; do not write from both sides at once.
 */
static int cmd_remount(const struct shell *sh, size_t argc, char **argv)
{
	int rc;

	if (fs_mnt.mnt_point == NULL) {
		shell_error(sh, "No file system was set up at boot");
		return -ENODEV;
	}

	(void)unmount_fs();

	rc = mount_fs();
	if (rc < 0) {
		shell_error(sh, "remount failed: %d", rc);
		return rc;
	}

	shell_print(sh, "Mounted %s (disk re-read)", fs_mnt.mnt_point);
	return 0;
}

SHELL_CMD_REGISTER(remount, NULL,
		   "Mount/re-read the disk FS to see files written by the USB host",
		   cmd_remount);
#endif /* CONFIG_SHELL */

int main(void)
{
	int ret;

	setup_disk();

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
	k_work_queue_init(&fs_refresh_q);
	k_work_queue_start(&fs_refresh_q, fs_refresh_stack,
			   K_THREAD_STACK_SIZEOF(fs_refresh_stack),
			   FS_REFRESH_PRIORITY, NULL);

	ret = enable_usb_device_next();
#else
	ret = usb_enable(NULL);
#endif
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return 0;
	}

	LOG_INF("The device is put in USB mass storage mode.\n");
	return 0;
}
