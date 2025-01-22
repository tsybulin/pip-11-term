#
# Makefile
#

CIRCLEHOME = ../..

OBJS	= main.o kernel.o firmware.o

LIBS	= $(CIRCLEHOME)/lib/libcircle.a \
          $(CIRCLEHOME)/lib/usb/libusb.a \
          $(CIRCLEHOME)/lib/input/libinput.a \
          $(CIRCLEHOME)/lib/fs/libfs.a \
          $(CIRCLEHOME)/app/lib/sdcard/libsdcard.a \
    	  $(CIRCLEHOME)/app/lib/fatfs/libfatfs.a \
          $(CIRCLEHOME)/lib/sched/libsched.a \
          $(CIRCLEHOME)/lib/net/libnet.a \
          $(CIRCLEHOME)/app/lib/util/libutil.a \
          $(CIRCLEHOME)/app/lib/cons/libcons.a

include ../Rules.mk

-include $(DEPS)
