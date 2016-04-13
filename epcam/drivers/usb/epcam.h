#ifndef __LINUX_epcam_H
#define __LINUX_epcam_H

#include <asm/uaccess.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
#include <linux/videodev.h>
#else
#include <linux/videodev2.h>
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
#include <media/v4l2-common.h>
#endif

#ifdef CONFIG_KERNEL_LOCK // LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
#include <linux/smp_lock.h>
#endif
#include <linux/input.h>


#ifdef epcam_DEBUG
#  define PDEBUG(level, fmt, args...) \
if (debug >= level) info("[%s:%d] " fmt, __PRETTY_FUNCTION__, __LINE__ , ## args)
#else
#  define PDEBUG(level, fmt, args...) do {} while(0)
#endif

#define debugprintk(level,x...) { if(epcam_debug) \
                                printk(level "EPCAM:" x);}

/* An almost drop-in replacement for sleep_on_interruptible */
#define wait_interruptible(test, noblock, queue, wait) \
{ \
	add_wait_queue(queue, wait); \
	set_current_state(TASK_INTERRUPTIBLE); \
	if (test) \
		schedule(); \
	remove_wait_queue(queue, wait); \
	set_current_state(TASK_RUNNING); \
	if (signal_pending(current)) \
		break; \
}


/* Helpers for reading/writing integers to the QT engine */
#define INT2QT(val, buf) { (buf)[0]=(val)&255; (buf)[1]=(val)/256; }
#define QT2INT(buf) ((buf)[0]+(buf)[1]*256)


/* EPCAM controls: */

#define VENDOR_REQ_CAMERA_INFO		0x00
#define VENDOR_REQ_CAPTURE_INFO		0x01
#define	VENDOR_REQ_COMPRESSION		0x02
#define VENDOR_REQ_CONT_CAPTURE		0x03
#define VENDOR_REQ_CAPTURE_FRAME	0x04
#define VENDOR_REQ_IMAGE_INFO		0x05
#define VENDOR_REQ_EXT_FEATURE		0x06
#define	VENDOR_REQ_CAM_POWER		0x07
#define VENDOR_REQ_LED_CONTROL		0x08
#define VENDOR_DEAD_PIXEL		0x09
#define VENDOR_REQ_AUTO_CONTROL		0x0a
#define VENDOR_REQ_BIOS			0xff

#define VENDOR_CMD_BIOS_READ		0x07

#define EPCAM_FORMAT_BAYER		1

/* Hyundai hv7131b registers
   7121 and 7141 should be the same (haven't really checked...) */
/* Mode registers: */
#define HV7131_REG_MODE_A	0x00
#define HV7131_REG_MODE_B	0x01
#define HV7131_REG_MODE_C	0x02
/* Frame registers: */
#define HV7131_REG_FRSU		0x10
#define HV7131_REG_FRSL		0x11
#define HV7131_REG_FCSU		0x12
#define HV7131_REG_FCSL		0x13
#define HV7131_REG_FWHU		0x14
#define HV7131_REG_FWHL		0x15
#define HV7131_REG_FWWU		0x16
#define HV7131_REG_FWWL		0x17
/* Timing registers: */
#define HV7131_REG_THBU		0x20
#define HV7131_REG_THBL		0x21
#define HV7131_REG_TVBU		0x22
#define HV7131_REG_TVBL		0x23
#define HV7131_REG_TITU		0x25
#define HV7131_REG_TITM		0x26
#define HV7131_REG_TITL		0x27
#define HV7131_REG_TMCD		0x28
/* Adjust Registers: */
#define HV7131_REG_ARLV		0x30
#define HV7131_REG_ARCG		0x31
#define HV7131_REG_AGCG		0x32
#define HV7131_REG_ABCG		0x33
#define HV7131_REG_APBV		0x34
#define HV7131_REG_ASLP		0x54
/* Offset Registers: */
#define HV7131_REG_OFSR		0x50
#define HV7131_REG_OFSG		0x51
#define HV7131_REG_OFSB		0x52
/* Reset level statistics registers: */
#define HV7131_REG_LOREFNOH	0x57
#define HV7131_REG_LOREFNOL	0x58
#define HV7131_REG_HIREFNOH	0x59
#define HV7131_REG_HIREFNOL	0x5a

#define EPCAM_V4L2_CID_GREEN_BALANCE (V4L2_CID_PRIVATE_BASE + 0)
#define EPCAM_V4L2_CID_RESET_LEVEL (V4L2_CID_PRIVATE_BASE + 1)
#define EPCAM_V4L2_CID_PIXEL_BIAS_VOLTAGE (V4L2_CID_PRIVATE_BASE + 2)
/* size of usb transfers */
#define EPCAM_PACKETBUFS	4
/* number of iso urbs to use */
#define EPCAM_NUMSBUF		2
/* read the usb specs for this one :) */
#define EPCAM_VIDEO_ENDPOINT	1
#define EPCAM_BUTTON_ENDPOINT	2
/* number of frames supported by the v4l part */
#define EPCAM_NUMFRAMES		8
/* scratch buffers for passing data to the decoders */
#define EPCAM_NUMSCRATCH	8
/* maximum amount of data in a JangGu packet */
#define EPCAM_VLCDATALEN	1024
/* number of nul sized packets to receive before kicking the camera */
#define EPCAM_MAX_NULLPACKETS	200
/* number of decoding errors before kicking the camera */
#define EPCAM_MAX_ERRORS	100
/* size of usb_make_path() buffer */
#define EPCAM_USB_PATH_LEN	64

#define EPCAM_MAX_EXPOSURE	0x259f00
#define	EPCAM_MIN_EXPOSURE	0x03f400

struct usb_device;

struct epcam_sbuf {
	unsigned char *data;
};

enum {
	FRAME_UNUSED,		/* Unused (no MCAPTURE) */
	FRAME_READY,		/* Ready to start grabbing */
	FRAME_GRABBING,		/* In the process of being grabbed into */
	FRAME_DONE,		/* Finished grabbing, but not been synced yet */
	FRAME_ERROR,		/* Something bad happened while processing */
};

enum {
	FMT_BAYER,
	FMT_JANGGU,
	FMT_EPLITE,
};

enum {
	BUFFER_UNUSED,
	BUFFER_READY,
	BUFFER_BUSY,
	BUFFER_DONE,
};

struct epcam_scratch {
	unsigned char *data;
	volatile int state;
	int offset;
	int length;
};

struct epcam_frame {
	unsigned char *data;		/* Frame buffer */

	volatile int grabstate;		/* State of grabbing */

	unsigned char *curline;
	int curlinepix;
	int curpix;
	struct v4l2_buffer buf;
	int vma_use_count;
};

struct usb_epcam {
	struct video_device vdev;

	/* Device structure */
	struct usb_device *dev;

	struct v4l2_device v4l2_dev;

	unsigned char iface;
	char usb_path[EPCAM_USB_PATH_LEN];

	char *camera_name;
	unsigned int camid;

	unsigned int exposure;
	unsigned int resetlevel;
	unsigned int pixelbiasvoltage;
	unsigned int chgsettings;
        unsigned int rgain;
        unsigned int ggain;
        unsigned int bgain;


	int format;
	int maxwidth;		/* max width */
	int maxheight;		/* max height */
	int cwidth;		/* current width */
	int cheight;		/* current height */
	int palette;
	int maxframesize;

	int removed;
	int user;		/* user count for exclusive use */

	int streaming;		/* Are we streaming video? */

	char *fbuf;		/* Videodev buffer area */

	int packetsize;
	struct urb *urb[EPCAM_NUMSBUF];
	struct urb *inturb;

	int button;
	int buttonpressed;

	int curframe;		/* Current receiving frame */
	struct epcam_frame frame[EPCAM_NUMFRAMES];
	int num_frames;
	int readcount;
	int framecount;

	int cancel;
	int dropped;
	int error;
	int underrun;
	int datacorrupt;

	int scratch_next;
	int scratch_use;
	int scratch_overflow;
	struct epcam_scratch scratch[EPCAM_NUMSCRATCH];
	int scratch_offset;

	/* Decoder specific data: */
	int lastoffset;
	int eplite_curpix;
	int eplite_curline;
	unsigned char eplite_data[1024];
	int vlc_size;
	int vlc_cod;
	int vlc_data;

	struct epcam_sbuf sbuf[EPCAM_NUMSBUF];
	struct input_dev *button_dev;


	wait_queue_head_t wq;	/* Processes waiting */
	struct mutex lock;
	struct mutex res_lock;

	/* proc interface */
	struct proc_dir_entry *proc_entry;	/* /proc/epcam/videoX */

	int nullpackets;
};


#endif
