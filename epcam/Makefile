#
# Graciously based on spca5xx Makefile (Michel Xhaard et all)
#
VERSION    = 1.4.1
CVSVERSION = "$Experimental work by Nuno Tavares 03/12/2004"
DEFINES    =

###
# The following flags enable experimental features.
# By default, these are enabled for development versions of the driver, and
# disabled for release versions.

# Optional: Enable driver debugging
DEFINES   += -Depcam_DEBUG

###
# Rest of Makefile follows here. You probably won't need to touch this.

# Setup defines
DEFINES   += -DCONFIG_USB_EPCAM_MODULE=1 -DMODULE -D__KERNEL__ 
DEFINES   += -DVID_HARDWARE_EPCAM=0xFF -DEPCAM_VERSION=\"$(VERSION)\"


# Setup the list of files to be included in a distribution
DIST_FILES = CHANGELOG \
             INSTALL \
             README \
             Makefile \
             drivers/usb/epcam.c \
             drivers/usb/epcam.h


ifneq ($(shell uname -r | cut -d. -f1,2), 2.4)

ifneq ($(KERNELRELEASE),)   # We were called by kbuild
EXTRA_CFLAGS += $(DEFINES) 
obj-m += epcam.o
epcam-objs := drivers/usb/epcam.o

else   # We were called from command line

KERNEL_VERSION = `uname -r`
KERNELDIR := /lib/modules/$(KERNEL_VERSION)/build
PWD  := $(shell pwd)
MODULE_INSTALLDIR = /lib/modules/$(KERNEL_VERSION)/kernel/drivers/media/usb

# Targets, don't change!
default:
	@echo '   Building EPCAM driver for '$(KERNEL_VERSION)' kernel.'
	@echo '   Remember: you must have read/write access to your kernel source tree.'
	$(MAKE) -C $(KERNELDIR) $(USER_OPT) M=$(PWD) modules

install:
#	mkdir -p $(MODULE_INSTALLDIR)
	install -c -m 0644 epcam.ko $(MODULE_INSTALLDIR)
	/sbin/depmod -a

uninstall:
	rm -f $(MODULE_INSTALLDIR)/epcam.ko
	/sbin/depmod -aq

dist:
	@echo Making distributable archives
	@rm -f epcam-src-$(VERSION).tar.gz
	@tar zcf epcam-src-$(VERSION).tar.gz $(DIST_FILES)



endif

else   # kernel version test

#############################################################################
# For Linux 2.4 users.
# Change the following lines according to your system configuration.
# It is important to configure your particular source tree ("make dep") before
# compiling this module!
#############################################################################
###
# This makefile will build the epcam driver module external to the kernel
# source tree. It makes it easier to swap kernels.


KERNEL_VERSION = `uname -r`

###
# Location of the header files (most importantly the config files)
# for the kernel you want to build the module against.
# This should be correct for the currently installed kernel on your machine.
KINCLUDE   = /lib/modules/$(KERNEL_VERSION)/build/include
KERNEL_ACFILE = $(KINCLUDE)/linux/autoconf.h
KERNEL_MODVERSIONSFILE = $(KINCLUDE)/linux/modversions.h
MODULE_INSTALLDIR = /lib/modules/$(KERNEL_VERSION)/kernel/drivers/usb/

# Detect module versioning support
ifneq ($(strip $(shell grep 'define CONFIG_MODVERSIONS 1' $(KERNEL_ACFILE))),)
	DEFINES += -DMODVERSIONS -include $(KERNEL_MODVERSIONSFILE)
endif

# Detect SMP support
ifneq ($(strip $(shell grep 'define CONFIG_SMP 1' $(KERNEL_ACFILE))),)
	DEFINES += -D__SMP__ -DSMP
endif

# Setup the tools
CC         = gcc
LD         = ld

# Setup compiler warnings
WARNINGS   = -Wall -Wpointer-arith
WARNINGS  += -Wcast-align -Wwrite-strings -Wstrict-prototypes
WARNINGS  += -Wuninitialized -Wreturn-type -Wunused -Wparentheses

# Setup compiler flags
EXTRA_CFLAGS     = -O2 -fomit-frame-pointer -fno-strict-aliasing -pipe
EXTRA_CFLAGS    += -mpreferred-stack-boundary=2 
EXTRA_CFLAGS    += -I$(KINCLUDE) -Idrivers/usb

# Setup link flags
LDFLAGS    = --strip-debug -r

OBJS       = drivers/usb/epcam.o

BINARY     = epcam.o

###
# Targets follow here

binary:	$(OBJS)
	@echo Linking $(BINARY)
	@$(LD) $(LDFLAGS) -o $(BINARY) $(OBJS)

install: binary
	@echo Installing.. Your root password may be required.
	su -c "make install-root"

install-root:
	@echo Installing..
	@mkdir -p /lib/modules/`uname -r`/kernel/drivers/usb
	@cp epcam.o /lib/modules/`uname -r`/kernel/drivers/usb/epcam.o
	@/sbin/depmod

dist:	clean binary
	@echo Making distributable archives
	@rm -f epcam-src-$(VERSION).tar.gz
	@tar zcf epcam-src-$(VERSION).tar.gz $(DIST_FILES)
	@rm -f epcam-module-$(VERSION).tar.gz
	@cp $(BINARY) epcam-$(VERSION).o
	@tar zcf epcam-module-$(VERSION).tar.gz epcam-$(VERSION).o README
	@rm epcam-$(VERSION).o

.c.o:	Makefile $*.c
	@echo Compiling $*.c
	@$(CC) $(EXTRA_CFLAGS) $(WARNINGS) $(DEFINES) -c $*.c -o $*.o

###
# Dependencies follow here

drivers/usb/epcam.o: drivers/usb/epcam.h

endif  # End kernel version test


############################################################################## 
# OTHER TARGETS 
##############################################################################
clean:
	rm -r -f drivers/usb/*.o \
	drivers/usb/.epcam.o.cmd  *.o *.ko *.mod.* .[a-r]* .[t-z]* core *.i *.symvers
	
############################################################################## 
