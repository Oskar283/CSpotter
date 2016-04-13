/*
 * Endpoints EPCAM USB Camera Driver
 *
 * Copyright (c) 2003, 2004 Jeroen B. Vreeken (pe1rxq@amsat.org)
 *
 * Based on the se401 driver, which in turn is based on the ov511 driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * Thanks to Endpoints Inc. (www.endpoints.com) for making documentation on
 * their chips available and sending me two reference boards.
 * 	- Jeroen Vreeken
 *
 * Modified by Fabio Mauro 2006 and Antoine DEBOURG
 * works fine with Ubuntu
 *
 * Modified by Djordje Stanarevic 2008 	- Version 0.08 vastly improved
 *                                      - tested with Creative PD1001 webcam
 *                                      - some code based on other linux drivers
 *                                        (stv680,quickcam_messanger,usbvideo)
 *                                      - Version 0.08.2 added sysfs support
 *					  for kernel 2.6.23 and older
 *					- Version 0.08.3 re-enabled reading of
 *					  data frames with epcam_read
 * Modified by Djordje Stanarevic 12/2008 - made changes introduced by kernel 2.6.27
 * 					    to v4l
 * Modified by Djordje Staarevic  11/2009 - implemented changes introduced after 2.6.27
 *                                          kernel.Added V4L2 support.
 * Modified by Djordje Stanarevic 11/2009 - implemented VIDIOC_ENUM_FRAMESIZES ioctl according to  V4L2
 *                                          specification.Camera should now  work with skype and ekiga
 *                                          although RGB24 to YU12 colorspace conversion in libv4lconvert.so works
 *                                          well only for the half of the captured image frame !??
 *
 * Modified by Djordje Stanarevic 12/2009 - modified VIDIOC_TRY_FMT handling to make ekiga work properly.
 *			                    ekiga has a bug that makes it crash when "Display images from
 *                                          your camera" button is enabled
 * Modified by Djordje Stanarevic 10/2010 - Changed pixel format to GBRG.Now supports only V4L2 specification.
 *                                          Tested on Creative PD1001a camera.
 *                                          Run skype with LD_PRELOAD=/usr/lib/libv4l/v4l2convert.so skype
 *                                          Ekiga,skype,vlc,cheese,amsn work with this driver.
 * Modified by Djordje Stanarevic 12/2010 - Implemented memory mapping i/o and input device for gizmo daemon.
 *                                          Tested with ekiga,skype,vlc,cheese,amsn,empathy,pidgin,ucview.
 *                                          Flash plugin doesn't support image format (doesn't use libv4l ??),
 *                                          google talk plugin crashes on VIDIOC_QUERYBUF (couldn't figure out why ?)
 * Modified by Djordje Stanarevic 01/2011 - Implemented HV7121b image sensor colour gain controls.
 *                                          Removed hue,saturation,contrast controls.
 *                                          Default values for red,blue,green balance and reset level are recommended,
 *                                          Values can be changed through Video4Linux Control Panel application.Some other
 *                                          applications alow changing default values (gyachi and ucview).
 * Modified by Djordje Stanarevic 02/2011 - Modified default values for red,blue,green colour balance and exposure.
 *                                          Implemented pixel bias voltage control.
 * Modified by Djordje Stanarevic 03/2011 - Page aligned frame buffers to get driver working with mplayer.Modified default
 *                                          values for red,blue and green balance.
 *                                          Run mplayer with LD_PRELOAD=/usr/lib/libv4l/v4l2convert.so mplayer ...
 * Modified by Djordje Stanarevic 03/2015 - Implemented changes required to support kernel 3.x.x (tested on 3.18.9)
 */

static const char version[] = "1.04.1";
#define MAJOR_VERSION	1
#define MINOR_VERSION	4
#define RELEASE_VERSION	1
#define __OLD_VIDIOC_	1
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/usb.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#include <linux/usb_input.h>
#else
#include <linux/usb/input.h>
#endif


#include "epcam.h"

static int video_nr=-1;
static int epcam_debug=0;

static void epcam_remove_disconnected(struct usb_epcam *epcam);
static inline int remap_page_range(struct vm_area_struct *vma,
unsigned long uvaddr,
unsigned long paddr,
unsigned long size, pgprot_t prot)
{
return remap_pfn_range(vma, uvaddr, paddr >> PAGE_SHIFT, size, prot);
}



static struct usb_device_id device_table [] = {
	{ USB_DEVICE(0x03e8, 0x1005), driver_info: (unsigned long)"Endpoints EP800"	},/* Reference model */
	{ USB_DEVICE(0x03e8, 0x1003), driver_info: (unsigned long)"Endpoints SE402"	},/* Reference model */
	{ USB_DEVICE(0x03e8, 0x1000), driver_info: (unsigned long)"Endpoints SE401"	},/* Reference model */
	{ USB_DEVICE(0x03e8, 0x2112), driver_info: (unsigned long)"SpyPen Actor" 	},
	{ USB_DEVICE(0x03e8, 0x2040), driver_info: (unsigned long)"Rimax Slim Multicam"	},
	{ USB_DEVICE(0x03e8, 0x1010), driver_info: (unsigned long)"Concord Eye-Q Easy"	},
	{ USB_DEVICE(0x041e, 0x400d), driver_info: (unsigned long)"Creative PD1001"	},
	{ USB_DEVICE(0x04f2, 0xa001), driver_info: (unsigned long)"Chicony DC-100"	},
	{ USB_DEVICE(0x08ca, 0x0102), driver_info: (unsigned long)"Aiptek Pencam 400"	},
	{ USB_DEVICE(0x03e8, 0x2182), driver_info: (unsigned long)"Concord EyeQ Mini"	},
	{ USB_DEVICE(0x03e8, 0x2123), driver_info: (unsigned long)"Sipix StyleCam"	},
	{}
};

MODULE_DEVICE_TABLE(usb, device_table);

MODULE_AUTHOR("Jeroen Vreeken <pe1rxq@amsat.org>, Djordje Stanarevic <dstanarevic@yahoo.com>");
MODULE_DESCRIPTION("EPcam USB Camera Driver");
MODULE_LICENSE("GPL");

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
module_param(video_nr, int, 0644);
module_param(epcam_debug, int, 0644);
#else
MODULE_PARM(video_nr, "i");
MODULE_PARM(epcam_debug, "i");
#endif

MODULE_DEVICE_TABLE(usb, device_table);
MODULE_PARM_DESC(video_nr,"Video device number /dev/video<nr> , default value is -1 (picks first available)");
MODULE_PARM_DESC(epcam_debug,"Debug messages, default value is 0 (no messages)");

static struct usb_driver epcam_driver;


/**********************************************************************
 *
 * Memory management
 *
 **********************************************************************/

/* Here we want the physical address of the memory.
 * This is used when initializing the contents of the area.
 */
static inline unsigned long kvirt_to_pa(unsigned long adr)
{
	unsigned long kva, ret;

	kva = (unsigned long) page_address(vmalloc_to_page((void *)adr));
	kva |= adr & (PAGE_SIZE-1); /* restore the offset */
	ret = __pa(kva);
	return ret;
}

static void *rvmalloc(unsigned long size)
{
	void *mem;
	unsigned long adr;

	size = PAGE_ALIGN(size);
	mem = vmalloc_32(size);
	if (!mem)
		return NULL;

	memset(mem, 0, size); /* Clear the ram out, no junk to the user */
	adr = (unsigned long) mem;
	while (size > 0) {
		SetPageReserved(vmalloc_to_page((void *)adr));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	return mem;
}

static void rvfree(void *mem, unsigned long size)
{
	unsigned long adr;

	if (!mem)
		return;

	adr = (unsigned long) mem;
	while ((long) size > 0) {
		ClearPageReserved(vmalloc_to_page((void *)adr));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	vfree(mem);
}

static int epcam_frames_alloc(struct usb_epcam *epcam,int number_of_frames)
{
	int i;
	unsigned int bufsize = PAGE_ALIGN(epcam->maxframesize);

	epcam->num_frames = number_of_frames;

	epcam->fbuf=rvmalloc(bufsize * number_of_frames);

	if(epcam->fbuf==NULL)
		return -ENOMEM;

	for(i=0; i < number_of_frames; i++)
	{
                epcam->frame[i].buf.index = i;
                epcam->frame[i].buf.m.offset = i * bufsize;
                epcam->frame[i].buf.length = epcam->maxframesize;
                epcam->frame[i].buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                epcam->frame[i].buf.sequence = 0;
                epcam->frame[i].buf.field = V4L2_FIELD_NONE;
                epcam->frame[i].buf.memory = V4L2_MEMORY_MMAP;
                epcam->frame[i].buf.flags = 0;
                epcam->frame[i].grabstate = FRAME_UNUSED;
                epcam->frame[i].data = epcam->fbuf + i * bufsize;
	}

	return number_of_frames;
}

static void epcam_frames_free(struct usb_epcam *epcam)
{
	unsigned int bufsize = PAGE_ALIGN(epcam->maxframesize);

	if(epcam->fbuf != NULL)
	{
		rvfree(epcam->fbuf,bufsize * epcam->num_frames);
		epcam->fbuf=0;
	}
}

static void epcam_empty_framequeues(struct usb_epcam *epcam)
{
	int i;

	for(i=0; i < epcam->num_frames;i++)
	{
		memset(&epcam->frame[i].buf,0,sizeof(struct v4l2_buffer));
		epcam->frame[i].grabstate = FRAME_UNUSED;
	}
	epcam->num_frames=0;
}

/****************************************************************************
 *
 * epcam register read/write functions
 *
 ***************************************************************************/

static int epcam_sndctrl(int set, struct usb_epcam *epcam, unsigned short req,
			 unsigned short value, unsigned char *cp, int size)
{
	return usb_control_msg (
                epcam->dev,
                set ? usb_sndctrlpipe(epcam->dev, 0) : usb_rcvctrlpipe(epcam->dev, 0),
                req,
                (set ? USB_DIR_OUT : USB_DIR_IN) | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
                value,
                0,
                cp,
                size,
                HZ
        );
}

static int epcam_set_feature(struct usb_epcam *epcam, unsigned short reg,
			     unsigned short value)
{
	unsigned char cp[2];
	INT2QT(value, cp);
	return usb_control_msg (
	epcam->dev,
		usb_sndctrlpipe(epcam->dev, 0),
		VENDOR_REQ_EXT_FEATURE,
		USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		reg,
		0,
		cp,
		2,
		HZ
	);
}

static unsigned short epcam_read_bios(struct usb_epcam *epcam,
					unsigned short address)
{
	unsigned char cp[2];
	usb_control_msg (
		epcam->dev,
		usb_rcvctrlpipe(epcam->dev, 0),
		VENDOR_REQ_BIOS,
		USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		VENDOR_CMD_BIOS_READ,
		address,
		cp,
		2,
		HZ
	);
	return QT2INT(cp);
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
/****************************************************************************
 *  sysfs
 ****************************************************************************/
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23)
#define epcam_file(name, variable, field)                              \
static ssize_t show_##name(struct device *class_dev,                    \
                           struct device_attribute *attr, char *buf)    \
{                                                                       \
        struct video_device *vdev = to_video_device(class_dev);         \
        struct usb_epcam *epcam = video_get_drvdata(vdev);              \
        return sprintf(buf, field, epcam->variable);                    \
}                                                                       \
static DEVICE_ATTR(name, S_IRUGO, show_##name, NULL);
#else
#define epcam_file(name, variable, field)                              \
static ssize_t show_##name(struct class_device *class_dev, char *buf)	\
{                                                                       \
        struct video_device *vdev = to_video_device(class_dev);         \
        struct usb_epcam *epcam = video_get_drvdata(vdev);              \
        return sprintf(buf, field, epcam->variable);                    \
}                                                                       \
static CLASS_DEVICE_ATTR(name, S_IRUGO, show_##name, NULL);
#endif
epcam_file(model, camera_name, "%s\n");
epcam_file(in_use, user, "%d\n");
epcam_file(streaming, streaming, "%d\n");
epcam_file(palette, palette, "%i\n");
epcam_file(frames_total, readcount, "%d\n");
epcam_file(frames_read, framecount, "%d\n");
epcam_file(packets_dropped, dropped, "%d\n");

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
static int epcam_create_sysfs_files(struct video_device *vdev)
{
        int rc;

        rc = device_create_file(&vdev->dev, &dev_attr_model);
        if (rc) goto err;
        rc = device_create_file(&vdev->dev, &dev_attr_in_use);
        if (rc) goto err_model;
        rc = device_create_file(&vdev->dev, &dev_attr_streaming);
        if (rc) goto err_inuse;
        rc = device_create_file(&vdev->dev, &dev_attr_palette);
        if (rc) goto err_stream;
        rc = device_create_file(&vdev->dev, &dev_attr_frames_total);
        if (rc) goto err_pal;
        rc = device_create_file(&vdev->dev, &dev_attr_frames_read);
        if (rc) goto err_framtot;
        rc = device_create_file(&vdev->dev, &dev_attr_packets_dropped);
        if (rc) goto err_framread;

        return 0;

        device_remove_file(&vdev->dev, &dev_attr_packets_dropped);
err_framread:
        device_remove_file(&vdev->dev, &dev_attr_frames_read);
err_framtot:
        device_remove_file(&vdev->dev, &dev_attr_frames_total);
err_pal:
        device_remove_file(&vdev->dev, &dev_attr_palette);
err_stream:
        device_remove_file(&vdev->dev, &dev_attr_streaming);
err_inuse:
        device_remove_file(&vdev->dev, &dev_attr_in_use);
err_model:
        device_remove_file(&vdev->dev, &dev_attr_model);
err:
        return rc;
}

static void epcam_remove_sysfs_files(struct video_device *vdev)
{
        device_remove_file(&vdev->dev, &dev_attr_model);
        device_remove_file(&vdev->dev, &dev_attr_in_use);
        device_remove_file(&vdev->dev, &dev_attr_streaming);
        device_remove_file(&vdev->dev, &dev_attr_palette);
        device_remove_file(&vdev->dev, &dev_attr_frames_total);
        device_remove_file(&vdev->dev, &dev_attr_frames_read);
        device_remove_file(&vdev->dev, &dev_attr_packets_dropped);
}
#else
static int epcam_create_sysfs_files(struct video_device *vdev)
{
        int rc;

        rc = video_device_create_file(vdev, &dev_attr_model);
        if (rc) goto err;
        rc = video_device_create_file(vdev, &dev_attr_in_use);
        if (rc) goto err_model;
        rc = video_device_create_file(vdev, &dev_attr_streaming);
        if (rc) goto err_inuse;
        rc = video_device_create_file(vdev, &dev_attr_palette);
        if (rc) goto err_stream;
        rc = video_device_create_file(vdev, &dev_attr_frames_total);
        if (rc) goto err_pal;
        rc = video_device_create_file(vdev, &dev_attr_frames_read);
        if (rc) goto err_framtot;
        rc = video_device_create_file(vdev, &dev_attr_packets_dropped);
        if (rc) goto err_framread;

        return 0;

        video_device_remove_file(vdev, &dev_attr_packets_dropped);
err_framread:
        video_device_remove_file(vdev, &dev_attr_frames_read);
err_framtot:
        video_device_remove_file(vdev, &dev_attr_frames_total);
err_pal:
        video_device_remove_file(vdev, &dev_attr_palette);
err_stream:
        video_device_remove_file(vdev, &dev_attr_streaming);
err_inuse:
        video_device_remove_file(vdev, &dev_attr_in_use);
err_model:
        video_device_remove_file(vdev, &dev_attr_model);
err:
        return rc;
}

static void epcam_remove_sysfs_files(struct video_device *vdev)
{
        video_device_remove_file(vdev, &dev_attr_model);
        video_device_remove_file(vdev, &dev_attr_in_use);
        video_device_remove_file(vdev, &dev_attr_streaming);
        video_device_remove_file(vdev, &dev_attr_palette);
        video_device_remove_file(vdev, &dev_attr_frames_total);
        video_device_remove_file(vdev, &dev_attr_frames_read);
        video_device_remove_file(vdev, &dev_attr_packets_dropped);
}
#endif
#else
static int epcam_create_sysfs_files(struct video_device *vdev)
{
        int rc;

        rc = video_device_create_file(vdev, &class_device_attr_model);
        if (rc) goto err;
        rc = video_device_create_file(vdev, &class_device_attr_in_use);
        if (rc) goto err_model;
        rc = video_device_create_file(vdev, &class_device_attr_streaming);
        if (rc) goto err_inuse;
        rc = video_device_create_file(vdev, &class_device_attr_palette);
        if (rc) goto err_stream;
        rc = video_device_create_file(vdev, &class_device_attr_frames_total);
        if (rc) goto err_pal;
        rc = video_device_create_file(vdev, &class_device_attr_frames_read);
        if (rc) goto err_framtot;
        rc = video_device_create_file(vdev, &class_device_attr_packets_dropped);
        if (rc) goto err_framread;

        return 0;

        video_device_remove_file(vdev, &class_device_attr_packets_dropped);
err_framread:
        video_device_remove_file(vdev, &class_device_attr_frames_read);
err_framtot:
        video_device_remove_file(vdev, &class_device_attr_frames_total);
err_pal:
        video_device_remove_file(vdev, &class_device_attr_palette);
err_stream:
        video_device_remove_file(vdev, &class_device_attr_streaming);
err_inuse:
        video_device_remove_file(vdev, &class_device_attr_in_use);
err_model:
        video_device_remove_file(vdev, &class_device_attr_model);
err:
        return rc;
}

static void epcam_remove_sysfs_files(struct video_device *vdev)
{
        video_device_remove_file(vdev, &class_device_attr_model);
        video_device_remove_file(vdev, &class_device_attr_in_use);
        video_device_remove_file(vdev, &class_device_attr_streaming);
        video_device_remove_file(vdev, &class_device_attr_palette);
        video_device_remove_file(vdev, &class_device_attr_frames_total);
        video_device_remove_file(vdev, &class_device_attr_frames_read);
        video_device_remove_file(vdev, &class_device_attr_packets_dropped);
}
#endif
#endif
/****************************************************************************
 *
 * Camera control
 *
 ***************************************************************************/

/* taken from usbvideo driver */
void epcam_hexdump(const unsigned char *data, int len)
{
        const int bytes_per_line = 32;
        char tmp[128]; /* 32*3 + 5 */
        int i, k;

        for (i=k=0; len > 0; i++, len--) {
                if (i > 0 && ((i % bytes_per_line) == 0)) {
                        debugprintk(KERN_INFO,"%s\n", tmp);
                        k=0;
                }
                if ((i % bytes_per_line) == 0)
                        k += sprintf(&tmp[k], "%04x: ", i);
                k += sprintf(&tmp[k], "%02x ", data[i]);
        }
        if (k > 0)
                debugprintk(KERN_INFO,"%s\n", tmp);
}

static void adjust_pict(struct usb_epcam *epcam)
{
	unsigned int exposure;
	unsigned int percentage;

	percentage=(epcam->exposure*100)>>16;
	exposure = (EPCAM_MAX_EXPOSURE)*percentage/100;
	exposure=min(exposure,(unsigned int)EPCAM_MAX_EXPOSURE);
	exposure=max(exposure,(unsigned int)EPCAM_MIN_EXPOSURE);
        epcam_set_feature(epcam, HV7131_REG_MODE_B,0x5);
	epcam_set_feature(epcam, HV7131_REG_TITU, exposure>>16);
	epcam_set_feature(epcam, HV7131_REG_TITM, (exposure>>8) & 0xff);
	epcam_set_feature(epcam, HV7131_REG_TITL, exposure & 0xff);
        epcam_set_feature(epcam, HV7131_REG_MODE_C,0x8);
        epcam_set_feature(epcam, HV7131_REG_ARCG, epcam->rgain);
        epcam_set_feature(epcam, HV7131_REG_AGCG, epcam->ggain);
        epcam_set_feature(epcam, HV7131_REG_ABCG, epcam->bgain);
        epcam_set_feature(epcam, HV7131_REG_ARLV, epcam->resetlevel);
        epcam_set_feature(epcam, HV7131_REG_APBV, epcam->pixelbiasvoltage);
	epcam->chgsettings=0;
}

/* irq handler for snapshot button */
static void epcam_button_irq(struct urb *urb)
{
        struct usb_epcam *epcam = urb->context;
        int status;
        int prevbtn=1;

        if(epcam->buttonpressed)
               prevbtn=0;

        debugprintk(KERN_INFO,"epcam_button_irq\n");
        if (!epcam->dev) {
                debugprintk(KERN_INFO,"ohoh: device vapourished\n");
                return;
        }

        switch (urb->status) {
        case 0:
                /* success */
                break;
        case -ECONNRESET:
        case -ENOENT:
        case -ESHUTDOWN:
                /* this urb is terminated, clean up */
                debugprintk(KERN_DEBUG,"%s - urb shutting down with status: %d", __FUNCTION__, urb->status);
                return;
        default:
                debugprintk(KERN_DEBUG,"%s - nonzero urb status received: %d", __FUNCTION__, urb->status);
                goto exit;
        }

        if (urb->actual_length >=2) {
                if (epcam->button)
                        epcam->buttonpressed=1;
                if(epcam->button_dev)
		{
                        debugprintk(KERN_INFO,"Snapshot button pressed\n");
	                input_report_key(epcam->button_dev,BTN_0,prevbtn);
                        input_sync(epcam->button_dev);
                        if(!prevbtn)
                                epcam->buttonpressed=0;
		}
        }
exit:
        status = usb_submit_urb (urb, GFP_ATOMIC);
        if (status)
                printk(KERN_ERR "%s - usb_submit_urb failed with result %d",
                     __FUNCTION__, status);
}

static void epcam_video_irq(struct urb *urb, struct pt_regs *regs)
{
	struct usb_epcam *epcam=urb->context;
	int length=0, i;

	/* ohoh... */
	if (!epcam->streaming) {
		return;
	}
	if (!urb) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
		info ("ohoh: null urb");
#else
		dev_info(&urb->dev->dev,"ohoh: null urb");
#endif
		return;
	}
	if (!epcam->dev) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
		info ("ohoh: device vapourished");
#else
		dev_info (&urb->dev->dev,"ohoh: device vapourished");
#endif
		return;
	}

	switch(epcam->scratch[epcam->scratch_next].state) {
		case BUFFER_READY:
		case BUFFER_BUSY: {
			epcam->dropped++;
			break;
		}
		case BUFFER_UNUSED: {
			for (i=0; i<EPCAM_PACKETBUFS; i++) {
				if (urb->iso_frame_desc[i].actual_length) {
					epcam->nullpackets=0;
					memcpy(
					 epcam->scratch[epcam->scratch_next].data+length,
					 (unsigned char *)urb->transfer_buffer+urb->iso_frame_desc[i].offset,
					 urb->iso_frame_desc[i].actual_length);
					length+=urb->iso_frame_desc[i].actual_length;

				}
			}
			if (length) {
				epcam->scratch[epcam->scratch_next].state=BUFFER_READY;
				epcam->scratch[epcam->scratch_next].offset=epcam->scratch_offset;
				epcam->scratch[epcam->scratch_next].length=length;
				if (waitqueue_active(&epcam->wq))
					wake_up_interruptible(&epcam->wq);
				epcam->scratch_overflow=0;
				epcam->scratch_next++;
				if (epcam->scratch_next>=EPCAM_NUMSCRATCH)
					epcam->scratch_next=0;
			}
		}
	}

	if (length)
		epcam->scratch_offset++;
	else
		epcam->scratch_offset=0;
	urb->dev = epcam->dev;
	if ((i = usb_submit_urb(urb, GFP_ATOMIC)) != 0)
	{
		debugprintk(KERN_INFO,"usb_submit_urb() returnd %d\n", i);
	}

	return;
}

static void epcam_send_size(struct usb_epcam *epcam, int width, int height)
{
	unsigned char cp[40];
	int compress=0;
	int size;

	compress=0x02;
	epcam->format=FMT_EPLITE;

	memset(cp,0,sizeof(cp));

	size = epcam_sndctrl(0, epcam, VENDOR_REQ_CAPTURE_INFO, 0, cp, sizeof(cp));
	debugprintk(KERN_INFO,"vendor_req_capture_info: %d\n", size);
	epcam_hexdump(cp,sizeof(cp));
	INT2QT(1, cp+2);
	INT2QT((epcam->maxwidth-width)/2, cp+4);
	INT2QT((epcam->maxheight-height)/2, cp+6);
	INT2QT(width, cp+8);
	INT2QT(height, cp+10);

	epcam_sndctrl(1, epcam, VENDOR_REQ_CAPTURE_INFO, 0, cp, size);
	size = epcam_sndctrl(0, epcam, VENDOR_REQ_CAPTURE_INFO, 0, cp, sizeof(cp));
	debugprintk(KERN_INFO,"vendor_req_capture_info: %d\n", size);
	epcam_hexdump(cp,sizeof(cp));
	debugprintk(KERN_INFO,"capture info size: %d\n", QT2INT(cp));
	debugprintk(KERN_INFO,"mode     : %d\n", QT2INT(cp+2));
	debugprintk(KERN_INFO,"xstart   : %d\n", QT2INT(cp+4));
	debugprintk(KERN_INFO,"ystart   : %d\n", QT2INT(cp+6));
	debugprintk(KERN_INFO,"width    : %d\n", QT2INT(cp+8));
	debugprintk(KERN_INFO,"height   : %d\n", QT2INT(cp+10));
	debugprintk(KERN_INFO,"framerate: %d\n", QT2INT(cp+12));
	debugprintk(KERN_INFO,"zoom     : %d\n", QT2INT(cp+14));

	memset(cp,0,sizeof(cp));
	debugprintk(KERN_INFO,"vendor_req_compression: %d\n", epcam_sndctrl(0, epcam, VENDOR_REQ_COMPRESSION, 0, cp, sizeof(cp)));
	INT2QT(compress, cp);
	epcam_sndctrl(1, epcam, VENDOR_REQ_COMPRESSION, 0, cp, 2);
	debugprintk(KERN_INFO,"compression: %d\n", compress);

	return;
}

static int epcam_start_stream(struct usb_epcam *epcam)
{
	struct urb *urb;
	int err=0, i, fx;
	unsigned int bufsize;

	epcam->streaming=1;

	debugprintk(KERN_INFO,"starting stream\n");

	bufsize = PAGE_ALIGN(epcam->maxframesize);
	/* Set the camera to Iso transfers */
	if (usb_set_interface(epcam->dev, epcam->iface, 4)< 0) {
		debugprintk(KERN_INFO,"interface set failed\n");
		return -1;
	}


	epcam->packetsize=epcam->dev->config[0].interface[0]->altsetting[4].endpoint[0].desc.wMaxPacketSize;
	debugprintk(KERN_INFO,"packetsize: %d\n", epcam->packetsize);

	epcam_sndctrl(1, epcam, VENDOR_REQ_CAM_POWER, 1, NULL, 0);
        epcam_sndctrl(1, epcam, VENDOR_REQ_LED_CONTROL, 1, NULL, 0);

	mutex_lock(&epcam->lock);
	epcam_send_size(epcam, epcam->cwidth, epcam->cheight);
	mutex_unlock(&epcam->lock);


	/* Do some memory allocation */
	for (i=0; i<epcam->num_frames; i++) {
		epcam->frame[i].data=epcam->fbuf + i * bufsize;
		epcam->frame[i].curpix=0;
	}
	for (i=0; i<EPCAM_NUMSBUF; i++) {
		epcam->sbuf[i].data=kmalloc(epcam->packetsize*EPCAM_PACKETBUFS, GFP_KERNEL);
		if(!epcam->sbuf[i].data) {
			for(i=i-1;i >= 0;i--) {
			kfree(epcam->sbuf[i].data);
			epcam->sbuf[i].data = NULL;
		}
		return -ENOMEM;
		}
	}

	epcam->scratch_offset=0;
	epcam->lastoffset=-1;
	epcam->scratch_next=0;
	epcam->scratch_use=0;
	epcam->scratch_overflow=0;
	for (i=0; i<EPCAM_NUMSCRATCH; i++) {
		epcam->scratch[i].data=kmalloc(epcam->packetsize*EPCAM_PACKETBUFS, GFP_KERNEL);
		if(!epcam->scratch[i].data) {
			for(i = i - 1; i >= 0; i--) {
				kfree(epcam->scratch[i].data);
				epcam->scratch[i].data = NULL;
			}
			goto nomem_sbuf;
		epcam->scratch[i].state=BUFFER_UNUSED;
	}
	}

	for (i=0; i<EPCAM_NUMSBUF; i++) {
		urb=usb_alloc_urb(EPCAM_PACKETBUFS, GFP_KERNEL);
		if(!urb) {
			for(i = i - 1;i >=0 ; i --) {
				usb_kill_urb(epcam->urb[i]);
				usb_free_urb(epcam->urb[i]);
				epcam->urb[i] = NULL;
			}
			goto nomem_scratch;
		}

		urb->dev=epcam->dev;
		urb->context=epcam;
		urb->pipe=usb_rcvisocpipe(epcam->dev, EPCAM_VIDEO_ENDPOINT);
		urb->transfer_flags=URB_ISO_ASAP;
		urb->transfer_buffer=epcam->sbuf[i].data;
		urb->complete=(usb_complete_t)epcam_video_irq;
		urb->number_of_packets=EPCAM_PACKETBUFS;
		urb->transfer_buffer_length=epcam->packetsize*EPCAM_PACKETBUFS;
		urb->interval = 1;
		for (fx=0; fx<EPCAM_PACKETBUFS; fx++) {
			urb->iso_frame_desc[fx].offset=epcam->packetsize*fx;
			urb->iso_frame_desc[fx].length=epcam->packetsize;
		}
		epcam->urb[i]=urb;
	}
	for (i=0; i<EPCAM_NUMSBUF; i++) {
		err=usb_submit_urb(epcam->urb[i], GFP_KERNEL);
		if(err)
			printk(KERN_ERR "usb_submit_urbed  error code %d",err);
	}
	debugprintk(KERN_INFO,"urbs flying!\n");
        /* Start interrupt transfers for snapshot button */
        epcam->inturb=usb_alloc_urb(0, GFP_KERNEL);
        if (!epcam->inturb) {
               debugprintk(KERN_INFO,"Allocation of inturb failed\n");
               return 1;
        }
        usb_fill_int_urb(epcam->inturb, epcam->dev,
               usb_rcvintpipe(epcam->dev, EPCAM_BUTTON_ENDPOINT),
               &epcam->button, sizeof(epcam->button),
               epcam_button_irq,
               epcam,
               8
        );
        if (usb_submit_urb(epcam->inturb, GFP_KERNEL)) {
               debugprintk(KERN_INFO,"int urb burned down\n");
               return 1;
        }
	epcam->framecount=0;
	epcam_sndctrl(1, epcam, VENDOR_REQ_CONT_CAPTURE, 1, NULL, 0);
	debugprintk(KERN_INFO,"capture on\n");
        adjust_pict(epcam);

	return 0;

nomem_scratch:
        for (i=0; i<EPCAM_NUMSCRATCH; i++) {
                kfree(epcam->scratch[i].data);
                epcam->scratch[i].data = NULL;
        }
 nomem_sbuf:
        for (i=0; i<EPCAM_NUMSBUF; i++) {
                kfree(epcam->sbuf[i].data);
                epcam->sbuf[i].data = NULL;
        }
        return -ENOMEM;

}

static int epcam_stop_stream(struct usb_epcam *epcam)
{
	int i;

	if (!epcam->streaming || !epcam->dev)
		return 1;

	epcam->streaming=0;

	for (i=0; i<EPCAM_NUMSBUF; i++) if (epcam->urb[i]) {
		usb_unlink_urb(epcam->urb[i]);
		usb_free_urb(epcam->urb[i]);
		epcam->urb[i]=NULL;
		kfree(epcam->sbuf[i].data);
	}
	for (i=0; i<EPCAM_NUMSCRATCH; i++) {
		kfree(epcam->scratch[i].data);
		epcam->scratch[i].data=NULL;
	}
	epcam_sndctrl(1, epcam, VENDOR_REQ_CONT_CAPTURE, 0, NULL, 0);
        epcam_sndctrl(1, epcam, VENDOR_REQ_LED_CONTROL, 0, NULL, 0);
	epcam_sndctrl(1, epcam, VENDOR_REQ_CAM_POWER, 0, NULL, 0);

	return 0;
}

static int epcam_set_size(struct usb_epcam *epcam, int width, int height)
{
	int wasstreaming=epcam->streaming;

	if((width==400 && height==300) ||
           (width==352 && height==288) ||
           (width==320 && height==240))
	{
	/* Check to see if we need to change */
	if (epcam->cwidth==width && epcam->cheight==height)
		return 0;

	/* Stop the current stream and set the new size */
	mutex_lock(&epcam->lock);
	if (wasstreaming)
		epcam_stop_stream(epcam);
	epcam->cwidth=width;
	epcam->cheight=height;
	mutex_unlock(&epcam->lock);

	return 0;
	}
	else
		return 1;
}


/****************************************************************************
 *
 * Video Decoding
 *
 ***************************************************************************/

static inline void decode_eplite_integrate(struct usb_epcam *epcam, int data)
{
	int linelength=epcam->cwidth;
	int i;
	char *framedata=epcam->frame[epcam->curframe].data;

	/* First two are absolute, all others relative.
	 */
	if (epcam->eplite_curpix < 2) {
		*(epcam->eplite_data+epcam->eplite_curpix)=1+data*4;
	} else {
		*(epcam->eplite_data+epcam->eplite_curpix)=i=
		    *(epcam->eplite_data+epcam->eplite_curpix-2)+data*4;
		if (i>255 || i<0) {
			epcam->lastoffset=-1;
			epcam->datacorrupt++;
		}
	}

	epcam->eplite_curpix++;

	if (epcam->eplite_curpix>=linelength) {
		memcpy(framedata + epcam->frame[epcam->curframe].curpix + 1,epcam->eplite_data,linelength);
                epcam->frame[epcam->curframe].curpix+=linelength;
		/* do we have complete frame ?? */
		if(epcam->frame[epcam->curframe].curpix >= epcam->cwidth * epcam->cheight) {
			for(i=0;i<epcam->cheight;i++)
			{
				*framedata=*(framedata+2);
				*(framedata+1)=*(framedata+3);
				framedata+=linelength;
			}
			epcam->frame[epcam->curframe].curpix=0;
			epcam->frame[epcam->curframe].grabstate=FRAME_DONE;
			epcam->framecount++;
			epcam->frame[epcam->curframe].buf.sequence = epcam->framecount;
			do_gettimeofday(&(epcam->frame[epcam->curframe].buf.timestamp));
			epcam->readcount++;

			if (epcam->frame[(epcam->curframe+1)&(epcam->num_frames-1)].grabstate==FRAME_READY) {
			epcam->curframe=(epcam->curframe+1) & (epcam->num_frames-1);
			}
		}

		if(epcam->chgsettings)
			adjust_pict(epcam);
		epcam->eplite_curpix=0;
		epcam->eplite_curline+=linelength;
		if (epcam->eplite_curline>=epcam->cheight*linelength)
			epcam->eplite_curline=0;
	}
}

static inline void decode_eplite (struct usb_epcam *epcam, struct epcam_scratch *buffer)
{
	unsigned char *data=buffer->data;
	int len=buffer->length;
	struct epcam_frame *frame=&epcam->frame[epcam->curframe];

	int pos=0;
	int vlc_cod;
	int vlc_size;
	int vlc_data;
	int bit_cur;

	int bit;

	/* Check for cancelled frames: */
	/*for (i=0; i<len-3; i++) {
		if (data[i]==0xff && data[i+1]==0xff && data[i+2]==0xff && data[i+3]==0xff) {
			epcam->cancel++;
			frame->curpix=0;
			return;
		}
	}*/

	/* New image? */
	if (!buffer->offset) {
		frame->curpix=0;
	}
	if (!frame->curpix) {
		epcam->eplite_curline=0;
		epcam->eplite_curpix=0;
		epcam->vlc_cod=0;
		epcam->vlc_data=0;
		epcam->vlc_size=0;
		if (frame->grabstate==FRAME_READY)
			frame->grabstate=FRAME_GRABBING;
	}

	vlc_cod=epcam->vlc_cod;
	vlc_size=epcam->vlc_size;
	vlc_data=epcam->vlc_data;

	while (pos < len && frame->grabstate==FRAME_GRABBING) {
		bit_cur=8;
		while (bit_cur) {
			bit=((*data)>>(bit_cur-1))&1;
			if (!vlc_cod) {
				if (bit) {
					vlc_size++;
				} else {
					if (!vlc_size) {
						decode_eplite_integrate(epcam, 0);
					} else {
						vlc_cod=2;
						vlc_data=0;
					}
				}
			} else {
				if (vlc_size > 7) {
					epcam->datacorrupt++;
					epcam->lastoffset=-1;
					return;
				}
				if (vlc_cod==2) {
					if (!bit) vlc_data=-(1<<vlc_size)+1;
					vlc_cod--;
				}
				vlc_size--;
				vlc_data+=bit<<vlc_size;
				if (!vlc_size) {
					decode_eplite_integrate(epcam, vlc_data);
					vlc_cod=0;
				}
			}
			bit_cur--;
		}
		pos++;
		data++;

	}

	/* Store variable-lengt-decoder state if in middle of frame */
	if (frame->grabstate==FRAME_GRABBING) {
		epcam->vlc_size=vlc_size;
		epcam->vlc_cod=vlc_cod;
		epcam->vlc_data=vlc_data;
	} else {
		/* If there is data left regard image as corrupt */
		if (len-pos > epcam->packetsize) {
			debugprintk(KERN_INFO,"len-pos: %d\n", len-pos);
			frame->grabstate=FRAME_GRABBING;
			epcam->datacorrupt++;
		}
		epcam->lastoffset=-1;
	}

}

static int epcam_newframe(struct usb_epcam *epcam, int framenr)
{
	DECLARE_WAITQUEUE(wait, current);

	while (epcam->streaming &&
	    (epcam->frame[framenr].grabstate==FRAME_READY ||
	     epcam->frame[framenr].grabstate==FRAME_GRABBING) ) {
		wait_interruptible(
		    epcam->scratch[epcam->scratch_use].state!=BUFFER_READY,
		    0,
		    &epcam->wq,
		    &wait
		);
		if (epcam->nullpackets > EPCAM_MAX_NULLPACKETS) {
			epcam->nullpackets=0;
			debugprintk(KERN_INFO,"to many null length packets, restarting capture\n");
			epcam_stop_stream(epcam);
		} else {
			struct epcam_scratch *buffer=&epcam->scratch[epcam->scratch_use];
			struct epcam_frame *frame=&epcam->frame[epcam->curframe];

			epcam->scratch[epcam->scratch_use].state=BUFFER_BUSY;

			/* New image? */
			if (!buffer->offset) {
				frame->curpix=0;
			}

			decode_eplite(epcam, &epcam->scratch[epcam->scratch_use]);

			epcam->scratch[epcam->scratch_use].state=BUFFER_UNUSED;
			epcam->scratch_use++;
			if (epcam->scratch_use>=EPCAM_NUMSCRATCH)
				epcam->scratch_use=0;
		}
	}
	if (epcam->frame[framenr].grabstate==FRAME_GRABBING)
		return -EAGAIN;

	return 0;
}

static void epcam_remove_disconnected(struct usb_epcam *epcam)
{
	int i;

        epcam->dev = NULL;

	for (i=0; i<EPCAM_NUMSBUF; i++) if (epcam->urb[i]) {
		usb_unlink_urb(epcam->urb[i]);
		usb_free_urb(epcam->urb[i]);
		epcam->urb[i] = NULL;
		kfree(epcam->sbuf[i].data);
	}
	for (i=0; i<EPCAM_NUMSCRATCH; i++) if (epcam->scratch[i].data) {
		kfree(epcam->scratch[i].data);
	}

	debugprintk(KERN_INFO,"%s disconnected\n", epcam->camera_name);

        /* Free the memory */
        if (!epcam->user) {
                kfree(epcam);
                epcam = NULL;
        }
}



/****************************************************************************
 *
 * Video4Linux
 *
 ***************************************************************************/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
static int epcam_open(struct inode *inode, struct file *file)
#else
static int epcam_open(struct file *file)
#endif
{
	struct video_device *dev = video_devdata(file);
	struct usb_epcam *epcam = (struct usb_epcam *)dev;

	debugprintk(KERN_INFO,"epcam_open()\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
#ifdef CONFIG_KERNEL_LOCK
	lock_kernel();
#endif
	if (epcam->user)
	{
#ifdef CONFIG_KERNEL_LOCK
		unlock_kernel();
#endif
		return -EBUSY;
	}
#else
	if (epcam->user)
		return -EBUSY;
#endif
	file->private_data = dev;

	epcam->user=1;
	debugprintk(KERN_INFO,"epcam_open() done\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
#ifdef CONFIG_KERNEL_LOCK
	unlock_kernel();
#endif
#endif

	return 0;
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
static int epcam_close(struct inode *inode, struct file *file)
#else
static int epcam_close(struct file *file)
#endif
{
	struct video_device *dev = video_devdata(file);
        struct usb_epcam *epcam = (struct usb_epcam *)dev;
	int i;

	debugprintk(KERN_INFO,"epcam_close()\n");
	if(dev == NULL)
		return -ENODEV;

	epcam_frames_free(epcam);
	epcam_empty_framequeues(epcam);
	if (epcam->removed) {
		epcam_remove_disconnected(epcam);
		debugprintk(KERN_INFO,"device unregistered\n");
	} else {
		for (i=0; i<epcam->num_frames; i++)
			epcam->frame[i].grabstate=FRAME_UNUSED;
		if (epcam->streaming)
			epcam_stop_stream(epcam);
	}
	epcam->user=0;
	file->private_data = NULL;
	debugprintk(KERN_INFO,"epcam_close() done\n");
	return 0;
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
static int epcam_do_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, void *arg)
#else
static int epcam_do_ioctl(struct file *file,
	unsigned int cmd, void *arg)
#endif
{
	struct video_device *vdev = file->private_data;
        struct usb_epcam *epcam = (struct usb_epcam *)vdev;

    if(epcam_debug)
		v4l_printk_ioctl(epcam->vdev.name,cmd);
	if(epcam->removed)
		return -ENODEV;
	if (!epcam->dev)
                return -EIO;
        switch (cmd) {
	case VIDIOC_QUERYCAP:
	{
		struct v4l2_capability *cap = arg;
		memset(cap,0,sizeof(*cap));
		strncpy(cap->driver,"epcam",sizeof (cap->driver));
		strncpy(cap->card,vdev->name, 32);
		strncpy(cap->bus_info,epcam->usb_path,sizeof(cap->bus_info));
		cap->version = KERNEL_VERSION(MAJOR_VERSION,MINOR_VERSION,RELEASE_VERSION);
		cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_READWRITE | V4L2_CAP_STREAMING;
		return 0;
	}
	case VIDIOC_QUERYCTRL:
	{
		struct v4l2_queryctrl *ctrl = arg;
		int ret=0;
                int id = ctrl->id;

                memset(ctrl, 0, sizeof(*ctrl));
                ctrl->id = id;

                switch (ctrl->id) {
                case V4L2_CID_EXPOSURE:
                	ctrl->type = V4L2_CTRL_TYPE_INTEGER;
                        strncpy(ctrl->name, "Exposure", sizeof(ctrl->name)-1);
                	ctrl->minimum = 0x00;
                	ctrl->maximum = 0xffff;
                	ctrl->step = 0x0001;
                	ctrl->default_value = 0x00;
			ctrl->flags = 0;
                        break;
                case V4L2_CID_RED_BALANCE:
                	ctrl->type = V4L2_CTRL_TYPE_INTEGER;
                        strncpy(ctrl->name, "Red Balance", sizeof(ctrl->name)-1);
                	ctrl->minimum = 0x00;
                	ctrl->maximum = 0x3f;
                	ctrl->step = 0x01;
                	ctrl->default_value = 0x35;
			ctrl->flags = 0;
                        break;
                case V4L2_CID_BLUE_BALANCE:
                	ctrl->type = V4L2_CTRL_TYPE_INTEGER;
                        strncpy(ctrl->name, "Blue Balance", sizeof(ctrl->name)-1);
                	ctrl->minimum = 0x00;
                	ctrl->maximum = 0x3f;
                	ctrl->step = 0x01;
                	ctrl->default_value = 0x35;
			ctrl->flags = 0;
                        break;
                case EPCAM_V4L2_CID_GREEN_BALANCE:
                	ctrl->type = V4L2_CTRL_TYPE_INTEGER;
                        strncpy(ctrl->name, "Green Balance", sizeof(ctrl->name)-1);
                	ctrl->minimum = 0x00;
                	ctrl->maximum = 0x3f;
                	ctrl->step = 0x01;
                	ctrl->default_value = 0x35;
			ctrl->flags = 0;
                        break;
                case EPCAM_V4L2_CID_RESET_LEVEL:
                	ctrl->type = V4L2_CTRL_TYPE_INTEGER;
                        strncpy(ctrl->name, "Reset Level", sizeof(ctrl->name)-1);
                	ctrl->minimum = 0x19;
                	ctrl->maximum = 0x3f;
                	ctrl->step = 0x01;
                	ctrl->default_value = 0x19;
			ctrl->flags = 0;
                        break;
                case EPCAM_V4L2_CID_PIXEL_BIAS_VOLTAGE:
                	ctrl->type = V4L2_CTRL_TYPE_INTEGER;
                        strncpy(ctrl->name, "Pixel Bias Voltage", sizeof(ctrl->name)-1);
                	ctrl->minimum = 0x00;
                	ctrl->maximum = 0x07;
                	ctrl->step = 0x01;
                	ctrl->default_value = 0x02;
			ctrl->flags = 0;
                        break;
		default:
			ret=-EINVAL;
                }

                return ret;

	}
	case VIDIOC_G_CTRL:
	{
		struct v4l2_control *ctrl = arg;
		int ret=0;

                switch (ctrl->id) {
                case V4L2_CID_EXPOSURE:
                        ctrl->value = epcam->exposure;
                        break;
                case V4L2_CID_RED_BALANCE:
                        ctrl->value = 0x3f - (epcam->rgain & 0x3f);
                        break;
                case V4L2_CID_BLUE_BALANCE:
                        ctrl->value = 0x3f - (epcam->bgain & 0x3f);
                        break;
                case EPCAM_V4L2_CID_GREEN_BALANCE:
                        ctrl->value = 0x3f - (epcam->ggain & 0x3f);
                        break;
                case EPCAM_V4L2_CID_RESET_LEVEL:
                        ctrl->value = epcam->resetlevel & 0x3f;
                        break;
                case EPCAM_V4L2_CID_PIXEL_BIAS_VOLTAGE:
                        ctrl->value = epcam->pixelbiasvoltage & 0x07;
                        break;
		default:
			ret = -EINVAL;
			break;
                }

		return ret;
	}
	case VIDIOC_S_CTRL:
	{
		struct v4l2_control *ctrl = arg;
		int ret = 0;

                switch (ctrl->id) {
                case V4L2_CID_EXPOSURE:
                	if (ctrl->value < 0x00 || ctrl->value > 0xffff)
                        	ret = -ERANGE;
			else
                        	epcam->exposure = ctrl->value;
                        break;
                case V4L2_CID_RED_BALANCE:
                	if (ctrl->value < 0x00 || ctrl->value > 0x3f)
                        	ret = -ERANGE;
			else
                        	epcam->rgain = 0x3f - ctrl->value;
                        break;
                case V4L2_CID_BLUE_BALANCE:
                	if (ctrl->value < 0x00 || ctrl->value > 0x3f)
                        	ret = -ERANGE;
			else
                        	epcam->bgain = 0x3f - ctrl->value;
                        break;
                case EPCAM_V4L2_CID_GREEN_BALANCE:
                	if (ctrl->value < 0x00 || ctrl->value > 0x3f)
                        	ret = -ERANGE;
			else
                        	epcam->ggain = 0x3f - ctrl->value;
                        break;
                case EPCAM_V4L2_CID_RESET_LEVEL:
                	if (ctrl->value < 0x19 || ctrl->value > 0x3f)
                        	ret = -ERANGE;
			else
                        	epcam->resetlevel = ctrl->value;
                        break;
                case EPCAM_V4L2_CID_PIXEL_BIAS_VOLTAGE:
                	if (ctrl->value < 0x02 || ctrl->value > 0x07)
                        	ret = -ERANGE;
			else
                        	epcam->pixelbiasvoltage = ctrl->value;
                        break;
		default:
			ret = -EINVAL;
			break;
                }
		if(ret==0)
			epcam->chgsettings = 1;
		return ret;
	}
	case VIDIOC_ENUMINPUT:
	{
		struct v4l2_input *input = arg;
		unsigned int index;
		if ( input->index > 0 || input->index < 0) {
                                return -EINVAL;
                }
		index = input->index;
		memset(input,0,sizeof(*input));
		input->index = index;
		input->type = V4L2_INPUT_TYPE_CAMERA;
		input->std = 0;
		strcpy(input->name ,"Composite");
		return 0;
	}
	case VIDIOC_G_INPUT:
	{
		int *input = arg;
		*input = 0;
		return 0;
	}
	case VIDIOC_S_INPUT:
	{
		int input = *(int *)arg;
		if(input < 0 || input > 0)
			return -EINVAL;
		return 0;
	}
	case VIDIOC_ENUM_FMT:
	{
		struct v4l2_fmtdesc *f = arg;
		unsigned int index;

		index = f->index;

		if(f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE || index !=0 )
			return -EINVAL;
		memset(f, 0, sizeof(*f));
		f->index = index;
	        f->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                f->pixelformat = V4L2_PIX_FMT_SGBRG8;
                f->description[0] = f->pixelformat & 0xff;
                f->description[1] = (f->pixelformat >> 8) & 0xff;
                f->description[2] = (f->pixelformat >> 16) & 0xff;
                f->description[3] = f->pixelformat >> 24;
                f->description[4] = '\0';
        	return 0;
	}
	case VIDIOC_TRY_FMT:
	{
		struct v4l2_format *fmt = arg;
		if(fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;
		if(fmt->fmt.pix.pixelformat != V4L2_PIX_FMT_SGBRG8)
			return -EINVAL;
		epcam_set_size(epcam,fmt->fmt.pix.width,fmt->fmt.pix.height);
		memset(fmt,0,sizeof(*fmt));
		fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_SGBRG8;
		fmt->fmt.pix.width = epcam->cwidth;
		fmt->fmt.pix.height = epcam->cheight;
		fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
		fmt->fmt.pix.field = V4L2_FIELD_NONE;
		fmt->fmt.pix.bytesperline = fmt->fmt.pix.width;
		fmt->fmt.pix.sizeimage = fmt->fmt.pix.height *  fmt->fmt.pix.bytesperline;
		fmt->fmt.pix.priv=0;
		return 0;
	}
	case VIDIOC_S_FMT:
	{
		struct v4l2_format *fmt = arg;
		if(fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;
		epcam_set_size(epcam,fmt->fmt.pix.width,fmt->fmt.pix.height);
		memset(fmt,0,sizeof(*fmt));
		fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_SGBRG8;
		fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
		fmt->fmt.pix.field = V4L2_FIELD_NONE;
		fmt->fmt.pix.width = epcam->cwidth;
		fmt->fmt.pix.height = epcam->cheight;
		fmt->fmt.pix.bytesperline = epcam->cwidth;
		fmt->fmt.pix.sizeimage = epcam->cheight *  fmt->fmt.pix.bytesperline;
		fmt->fmt.pix.priv=0;
		return 0;

	}
	case VIDIOC_G_FMT:
	{
		struct v4l2_format *fmt = arg;
		if(fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;
		memset(fmt,0,sizeof(*fmt));
		fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt->fmt.pix.width = epcam->cwidth;
		fmt->fmt.pix.height = epcam->cheight;
		fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_SGBRG8;
		fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
		fmt->fmt.pix.bytesperline = fmt->fmt.pix.width;
		fmt->fmt.pix.sizeimage = fmt->fmt.pix.height *  fmt->fmt.pix.bytesperline;
		fmt->fmt.pix.field = V4L2_FIELD_NONE;

		return 0;
	}
	case VIDIOC_ENUM_FRAMESIZES:
	{
		struct v4l2_frmsizeenum *frm = arg;
		unsigned int index = frm->index;
		int retval=0;
		if(frm->index < 0 || frm->index > 2 || frm->pixel_format != V4L2_PIX_FMT_SGBRG8)
			return -EINVAL;
		memset(frm,0,sizeof(*frm));
		frm->index = index;
		frm->pixel_format = V4L2_PIX_FMT_SGBRG8;
		frm->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		switch(frm->index)
		{
			case 0:
				frm->discrete.width = 320;
				frm->discrete.height = 240;
				break;
			case 1:
				frm->discrete.width = 352;
				frm->discrete.height = 288;
				break;
			case 2:
				frm->discrete.width = 400;
				frm->discrete.height = 300;
				break;
			default:
				retval = -EINVAL;
				break;
		}
		return retval;
	}
	case VIDIOC_REQBUFS:
	{
		struct v4l2_requestbuffers *rb = arg;
		int retval=0;
		int count=0;

		if((rb->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) ||
		(rb->memory != V4L2_MEMORY_MMAP))
			return -EINVAL;

		count = rb->count;
		debugprintk(KERN_INFO,"VIDIOC_REQBUFS: In count %d\n",count);

		if(count > EPCAM_NUMFRAMES || count < 0)
			count=EPCAM_NUMFRAMES;

		memset(rb,0,sizeof(*rb));

		if(epcam->streaming)
			epcam_stop_stream(epcam);

		rb->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		rb->memory = V4L2_MEMORY_MMAP;


		epcam_frames_free(epcam);
		epcam_empty_framequeues(epcam);
		if(count)
		{
			rb->count = epcam_frames_alloc(epcam,count);
		}

		epcam->curframe = 0;

		if(rb->count < 0)
		{
			retval = rb->count;
			rb->count=0;
		}

		debugprintk(KERN_INFO,"VIDIOC_REQBUFS: Out count %d\n",rb->count);
		return retval;

	}
	case VIDIOC_QUERYBUF:
	{
		struct v4l2_buffer *buf = arg;
		struct epcam_frame *frame;
		int retval=0;
		int index = buf->index;

		if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;

		if(buf->index>=epcam->num_frames)
			return -EINVAL;


		debugprintk(KERN_INFO,"VIDIOC_QUERYBUF: Index %d\n",index);
		memcpy(buf,&epcam->frame[index].buf,sizeof *buf);


		/* Updating the corresponding frame state */
		buf->flags = 0;
		frame = &epcam->frame[buf->index];
		if(frame->grabstate >= FRAME_READY)
		{
			debugprintk(KERN_INFO "VIDIOC_QUERYBUF:QUEUED\n");
			buf->flags |= V4L2_BUF_FLAG_QUEUED;
		}
		if(frame->grabstate >= FRAME_DONE)
		{
			debugprintk(KERN_INFO "VIDIOC_QUERYBUF:DONE\n");
			buf->flags |= V4L2_BUF_FLAG_DONE;
		}
		if(frame->vma_use_count)
		{
			debugprintk(KERN_INFO "VIDIOC_QUERYBUF:MAPPED\n");
			buf->flags |= V4L2_BUF_FLAG_MAPPED;
		}

		return retval;

	}
	case VIDIOC_QBUF:
	{
		struct v4l2_buffer *buf = arg;
		struct epcam_frame *frame;
		int retval=0;

		if ((buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) ||
		(buf->memory != V4L2_MEMORY_MMAP))
			return -EINVAL;

		if(buf->index>=epcam->num_frames)
			return -EINVAL;

		frame = &epcam->frame[buf->index];

		if(frame->grabstate != FRAME_UNUSED)
			return -EAGAIN;

		frame->grabstate = BUFFER_READY;
		buf->flags = V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_QUEUED;
		buf->flags |= ~V4L2_BUF_FLAG_DONE;

		return retval;
	}
	case VIDIOC_DQBUF:
	{
		struct v4l2_buffer *buf = arg;
		struct epcam_frame *frame;
		int retval=0;

		if ((buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) ||
		(buf->memory != V4L2_MEMORY_MMAP) || !epcam->streaming)
			return -EINVAL;

		debugprintk(KERN_INFO,"DQBUF : %s\n",file->f_flags & O_NONBLOCK ? "Nonblocking" : "Blocking");
		retval = epcam_newframe(epcam,0);

		if(retval)
			return retval;

		frame=&epcam->frame[0];

		frame->grabstate = FRAME_UNUSED;

		buf->memory = V4L2_MEMORY_MMAP;
		buf->flags = V4L2_BUF_FLAG_MAPPED |
			V4L2_BUF_FLAG_QUEUED |
			V4L2_BUF_FLAG_DONE;
		buf->index = 0;
		buf->sequence = frame->buf.sequence;
		buf->timestamp = frame->buf.timestamp;
		buf->field = V4L2_FIELD_NONE;
		buf->bytesused = epcam->cwidth * epcam->cheight;

		return retval;
	}
	case VIDIOC_STREAMON:
	{
		int *type = arg;
		int retval=0;

		if (*type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;

		if (!epcam->streaming)
			epcam_start_stream(epcam);


		return retval;
	}
	case VIDIOC_STREAMOFF:
	{
		int *type = arg;
		int retval=0;

		if (*type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;

		epcam_stop_stream(epcam);


		return retval;
	}
	default:
	{
		int retval=-ENOIOCTLCMD;
        debugprintk(KERN_INFO, "Unsupported ioctl 0x%08x\n", cmd );
        return retval;
	}
        } /* end switch */

        return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
static long epcam_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	return video_usercopy(inode, file, cmd, arg, epcam_do_ioctl);
}
#else
static long epcam_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	return video_usercopy(file, cmd, arg, (v4l2_kioctl)epcam_do_ioctl);
}
#endif

static ssize_t epcam_read(struct file *file, char *buf,
	size_t count, loff_t *ppos)
{
	int realcount=count, ret=0;
	struct video_device *dev = file->private_data;
	struct usb_epcam *epcam = (struct usb_epcam *)dev;

	if (epcam->removed)
		return -ENODEV;
	if(epcam->framecount < 10)
		debugprintk(KERN_INFO,"In epcam_read\n");
	if (epcam->dev == NULL)
		return -ENODEV;

	if(!epcam->num_frames)
	{
		epcam_frames_free(epcam);
		epcam_empty_framequeues(epcam);
		epcam_frames_alloc(epcam,EPCAM_NUMFRAMES);
	}
	if (realcount > epcam->cwidth*epcam->cheight)
		realcount=epcam->cwidth*epcam->cheight;

	if (!(epcam->frame[0].grabstate==FRAME_GRABBING)) {
		epcam->frame[0].grabstate=FRAME_READY;
		epcam->frame[1].grabstate=FRAME_UNUSED;
		epcam->curframe=0;
	}
	else
		return -EBUSY;

	if (!epcam->streaming)
		epcam_start_stream(epcam);

        if (epcam->framecount==0)
                adjust_pict(epcam);

	ret=epcam_newframe(epcam, 0);

	if (ret == -EAGAIN)
		return ret;
	if (!ret) {
		ret = copy_to_user(buf, epcam->frame[0].data, realcount);
	} else {
		realcount=ret;
	}

	epcam->frame[0].grabstate=FRAME_UNUSED;
	return realcount;
}

static void epcam_vm_open(struct vm_area_struct *vma)
{
        struct epcam_frame *buffer = vma->vm_private_data;
        debugprintk(KERN_INFO, "epcam_vm_open\n");
        buffer->vma_use_count++;
}

static void epcam_vm_close(struct vm_area_struct *vma)
{
        struct epcam_frame *buffer = vma->vm_private_data;
        debugprintk(KERN_INFO, "epcam_vm_close\n");
        buffer->vma_use_count--;
}

static struct vm_operations_struct epcam_vm_ops = {
        .open           = epcam_vm_open,
        .close          = epcam_vm_close,
};

static int epcam_v4l2_mmap(struct file *file, struct vm_area_struct *vma)
{
        struct video_device *vdev = video_devdata(file);
        struct usb_epcam *epcam = video_get_drvdata(vdev);
        struct epcam_frame *uninitialized_var(buffer);
        struct page *page;
        unsigned long addr, start, size;
        unsigned int i;
        int ret = 0;

        debugprintk(KERN_INFO, "epcam_v4l2_mmap\n");

        start = vma->vm_start;
        size = vma->vm_end - vma->vm_start;

        mutex_lock(&epcam->lock);

	if(!(vma->vm_flags & VM_WRITE))
	{
       		debugprintk(KERN_INFO, "epcam_v4l2_mmap: vm_flags %lu\n",vma->vm_flags);
                goto done;
	}

        for (i = 0; i < epcam->num_frames; ++i) {
                buffer = &epcam->frame[i];
                if ((buffer->buf.m.offset >> PAGE_SHIFT) == vma->vm_pgoff)
                        break;
        }

        if (i == epcam->num_frames || size != PAGE_ALIGN(epcam->maxframesize)) {
        	debugprintk(KERN_INFO, "epcam_v4l2_mmap: EINVAL index %d size %lu framesize %d\n",
				i,
				size,
				epcam->maxframesize);
                ret = -EINVAL;
                goto done;
        }

        vma->vm_flags |= VM_IO;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
        vma->vm_flags |= VM_RESERVED;
#else
        vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);
#endif

        addr = (unsigned long)epcam->frame[i].data;
        while (size > 0) {
                page = vmalloc_to_page((void *)addr);
                if ((ret = vm_insert_page(vma, start, page)) < 0)
		{
        		debugprintk(KERN_INFO, "epcam_v4l2_mmap: vm_insert_page failed %d\n",ret);
                        goto done;
		}

                start += PAGE_SIZE;
                addr += PAGE_SIZE;
                size -= PAGE_SIZE;
        }

        vma->vm_ops = &epcam_vm_ops;
        vma->vm_private_data = buffer;
        epcam_vm_open(vma);

done:
        mutex_unlock(&epcam->lock);
        return ret;
}

/* release callback too suppress kernel warning message */
void epcam_vdev_release(struct video_device *vdev)
{
	debugprintk(KERN_INFO,"Video device release callback\n");
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
static struct file_operations epcam_fops = {
#else
static struct v4l2_file_operations epcam_fops = {
#endif
	.owner =	THIS_MODULE,
	.open =		epcam_open,
	.release =	epcam_close,
	.read =		epcam_read,
	.mmap =		epcam_v4l2_mmap,
	.ioctl =	epcam_ioctl,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
#ifdef CONFIG_COMPAT
	.compat_ioctl = v4l_compat_ioctl32,
#endif
	.llseek = 	no_llseek,
#endif
};

static struct video_device epcam_template = {
        .name =		"EPCAM USB camera",
	.fops =		&epcam_fops,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
	.type = 	VID_TYPE_CAPTURE | VID_TYPE_SCALES,
	.release=	epcam_vdev_release,
	.minor=		-1
#else
	.release=	video_device_release_empty
#endif
};



/***************************/

static int epcam_init(struct usb_epcam *epcam)
{
        int i=0, rc;
        unsigned char cp[0x80];

        epcam_sndctrl(1, epcam, VENDOR_REQ_LED_CONTROL, 1, NULL, 0);

	/* get camera descriptor */
	memset(cp, 0, 0x80);
	rc=epcam_sndctrl(0, epcam, VENDOR_REQ_CAMERA_INFO, 0, cp, sizeof(cp));
	debugprintk(KERN_INFO,"vendor_req_camera_info: %d\n", rc);
	if (rc<0) {
		printk(KERN_ERR "EPCAM:Error reading camera descriptor\n");
		return 1;
	}
	debugprintk(KERN_INFO,"size     :%d  %x %x\n", QT2INT(cp   ), cp[0], cp[1]);
	debugprintk(KERN_INFO,"rev      :%d  %x %x\n", QT2INT(cp+4 ), cp[4], cp[5]);
	debugprintk(KERN_INFO,"maxwidth :%d  %x %x\n", QT2INT(cp+6 ), cp[6], cp[7]);
	debugprintk(KERN_INFO,"maxheight:%d  %x %x\n", QT2INT(cp+8 ), cp[8], cp[9]);
	debugprintk(KERN_INFO,"zoomcaps :%d  %x %x\n", QT2INT(cp+10), cp[10], cp[11]);
	debugprintk(KERN_INFO,"ISPCaps  :%d  %x %x\n", QT2INT(cp+12), cp[12], cp[13]);
	debugprintk(KERN_INFO,"Formats  :%d  %x %x\n", QT2INT(cp+14), cp[14], cp[15]);
	for (i=0; i<QT2INT(cp+14); i++) {
		if (QT2INT(cp+16+i*2)==EPCAM_FORMAT_BAYER)
			printk(KERN_INFO "EPCAM:Bayer format supported\n");
		else
			printk(KERN_INFO "EPCAM:Unknown format %d\n", QT2INT(cp+16+i*2));
	}
	debugprintk(KERN_INFO,"bios version: %d\n", epcam_read_bios(epcam, 0xfffc));

	epcam->maxwidth=QT2INT(cp+6);
	epcam->maxheight=QT2INT(cp+8);
	epcam->camid=QT2INT(cp+2);

	debugprintk(KERN_INFO,"camid: %x\n", epcam->camid);
	if (epcam->camid!=0x800 &&
	    epcam->camid!=0x402 &&
	    epcam->camid!=0x401 ) {
		printk(KERN_ERR "Not a supported camid: %d!", epcam->camid);
		return 1;
	}
	if (epcam->camid==0x401) {
		debugprintk(KERN_INFO,"Your camera might work with this driver...\n");
		debugprintk(KERN_INFO,"Please send your results to: pe1rxq@amsat.org\n");
	}

	epcam->cwidth=epcam->maxwidth;
	epcam->cheight=epcam->maxheight;
	epcam->maxframesize=(epcam->maxwidth)*(epcam->maxheight);


	/* some default values */
	epcam->dropped=0;
	epcam->framecount=0;
	epcam->error=0;
	epcam->readcount=0;
	epcam->streaming =0;
        epcam->exposure = 0x00;
	epcam->rgain = 0x0a;
	epcam->ggain = 0x0a;
	epcam->bgain = 0x0a;
	epcam->resetlevel = 0x19;
	epcam->pixelbiasvoltage = 0x02;

        /* Start interrupt transfers for snapshot button */
        epcam->inturb=usb_alloc_urb(0, GFP_KERNEL);
        if (!epcam->inturb) {
               debugprintk(KERN_INFO,"Allocation of inturb failed\n");
               return 1;
        }
        usb_fill_int_urb(epcam->inturb, epcam->dev,
               usb_rcvintpipe(epcam->dev, EPCAM_BUTTON_ENDPOINT),
               &epcam->button, sizeof(epcam->button),
               epcam_button_irq,
               epcam,
               8
        );
        if (usb_submit_urb(epcam->inturb, GFP_KERNEL)) {
               debugprintk(KERN_INFO,"int urb burned down\n");
               return 1;
        }

	/* Flash the led */
        epcam_sndctrl(1, epcam, VENDOR_REQ_CAM_POWER, 1, NULL, 0);
	epcam_sndctrl(1, epcam, VENDOR_REQ_LED_CONTROL, 1, NULL, 0);
        epcam_sndctrl(1, epcam, VENDOR_REQ_CAM_POWER, 0, NULL, 0);
	epcam_sndctrl(1, epcam, VENDOR_REQ_LED_CONTROL, 0, NULL, 0);


        return 0;
}

static int epcam_probe(struct usb_interface *intf,
	const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
        struct usb_interface_descriptor *interface;
        struct usb_epcam *epcam;
        char *camera_name=NULL;
	char *phys = NULL;
	int ret=0;


	if (!id)
		return -ENODEV;
        if (dev->descriptor.bNumConfigurations != 1)
                return -ENODEV;

	interface = &intf->altsetting[0].desc;

	if (interface->bInterfaceNumber!=0)
		return -ENODEV;

	camera_name=(char *)id->driver_info;

        /* We found one */
        printk(KERN_INFO "EPCAM:camera found -> %s\n", camera_name);

        if ((epcam = kmalloc(sizeof(*epcam), GFP_KERNEL)) == NULL) {
                printk(KERN_ERR "couldn't kmalloc epcam struct");
                return -ENODEV;
        }

        memset(epcam, 0, sizeof(*epcam));


        epcam->dev = dev;
        epcam->iface = interface->bInterfaceNumber;
        epcam->camera_name = camera_name;
	debugprintk(KERN_INFO,"firmware version: %02x\n", dev->descriptor.bcdDevice & 255);

	    ret = v4l2_device_register(&intf->dev, &epcam->v4l2_dev);
	    if( ret < 0)
	    {
	    	printk(KERN_ERR "couldn't register v4l2_device\n");
	    	kfree(epcam);
	    	return ret;
	    }

        if (epcam_init(epcam)) {
  		v4l2_device_unregister(&epcam->v4l2_dev);
		kfree(epcam);
		return -ENODEV;
	}


	memcpy(&epcam->vdev, &epcam_template, sizeof(epcam_template));
	memcpy(epcam->vdev.name, epcam->camera_name, strlen(epcam->camera_name));
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
	epcam->vdev.dev = &dev->dev;
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
	epcam->vdev.parent = &dev->dev;
#else
	epcam->vdev.v4l2_dev = &epcam->v4l2_dev;
#endif
#endif
	init_waitqueue_head(&epcam->wq);
	mutex_init(&epcam->lock);
	wmb();

        if (usb_make_path(dev, epcam->usb_path, EPCAM_USB_PATH_LEN) < 0) {
                printk(KERN_ERR "usb_make_path error");
                v4l2_device_unregister(&epcam->v4l2_dev);
                kfree(epcam);
                return -ENODEV;
        }

	video_set_drvdata(&epcam->vdev,epcam);

	if (video_register_device(&epcam->vdev, VFL_TYPE_GRABBER, video_nr) == -1) {
		printk(KERN_ERR "video_register_device failed");
    	v4l2_device_unregister(&epcam->v4l2_dev);
		kfree(epcam);
		return -ENODEV;
	}
        debugprintk(KERN_INFO,"Device at %s registered to minor %d\n", epcam->usb_path,
             epcam->vdev.minor);
	debugprintk(KERN_INFO,"registered new video device: video%d\n", epcam->vdev.minor);
	usb_set_intfdata(intf, epcam);

	ret = epcam_create_sysfs_files(&epcam->vdev);
	if(ret) {
		video_unregister_device(&epcam->vdev);
		if(epcam->button_dev)
		{
			input_unregister_device(epcam->button_dev);
			input_free_device(epcam->button_dev);
			kfree(epcam->button_dev->phys);
			epcam->button_dev=NULL;
		}
		video_device_release(&epcam->vdev);
    	v4l2_device_unregister(&epcam->v4l2_dev);
		kfree(epcam);
		return ret;
	}

	/* register input button device for snapshots */
	epcam->button_dev = input_allocate_device();
        if (!epcam->button_dev) {
                debugprintk(KERN_INFO,"Insufficient memory for webcam snapshot button device.");
                return -ENOMEM;
        }

        epcam->button_dev->name = "EPCAM snapshot button";
        phys = kasprintf(GFP_KERNEL,"usb-%s-%s", epcam->dev->bus->bus_name, epcam->dev->devpath);
        if (!phys) {
                input_free_device(epcam->button_dev);
                return -ENOMEM;
        }
        epcam->button_dev->phys = phys;
        usb_to_input_id(epcam->dev, &epcam->button_dev->id);
        epcam->button_dev->dev.parent = &epcam->dev->dev;
        epcam->button_dev->evbit[0] = BIT_MASK(EV_KEY);
        epcam->button_dev->keybit[BIT_WORD(BTN_0)] = BIT_MASK(BTN_0);

        ret = input_register_device(epcam->button_dev);
        if (ret) {
                input_free_device(epcam->button_dev);
                kfree(epcam->button_dev->phys);
                epcam->button_dev = NULL;
                return ret;
        }
        return 0;
}

static void epcam_disconnect(struct usb_interface *intf)
{
	struct usb_epcam *epcam = usb_get_intfdata(intf);

	debugprintk(KERN_INFO,"camera disconnected\n");
	usb_set_intfdata(intf, NULL);
	if (epcam) {
		debugprintk(KERN_INFO,"unregistering video device\n");
        	epcam_remove_sysfs_files(&epcam->vdev);
		video_unregister_device(&epcam->vdev);
        v4l2_device_disconnect(&epcam->v4l2_dev);
		if(epcam->button_dev)
		{
			input_unregister_device(epcam->button_dev);
			input_free_device(epcam->button_dev);
			kfree(epcam->button_dev->phys);
			epcam->button_dev=NULL;
		}
		if (!epcam->user)
			epcam_remove_disconnected(epcam);
		else {
		        epcam->frame[0].grabstate = FRAME_ERROR;
		        epcam->frame[1].grabstate = FRAME_ERROR;

			epcam->streaming = 0;

	                wake_up_interruptible(&epcam->wq);
			epcam->removed = 1;
		}
        v4l2_device_put(&epcam->v4l2_dev);
	}
}


static struct usb_driver epcam_driver = {
	.name		= "epcam",
	.id_table	= device_table,
	.probe		= epcam_probe,
	.disconnect	= epcam_disconnect,
};



/****************************************************************************
 *
 *  Module routines
 *
 ***************************************************************************/

static int __init usb_epcam_init(void)
{
	debugprintk(KERN_INFO,"usb camera driver version %s registering\n", version);
	return usb_register(&epcam_driver);
}

static void __exit usb_epcam_exit(void)
{
	usb_deregister(&epcam_driver);
	debugprintk(KERN_INFO,"driver deregistered\n");
}

module_init(usb_epcam_init);
module_exit(usb_epcam_exit);
