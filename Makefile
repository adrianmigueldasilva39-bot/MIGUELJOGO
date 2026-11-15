TARGET = slither
OBJS = main.o
PSL1GHT ?= $(shell psl1ght-config --prefix 2>/dev/null || echo /usr/local/ps3dev)
ifeq ($(PSL1GHT),)
PSL1GHT = /usr/local/ps3dev
endif
include $(PSL1GHT)/ppu_rules
CFLAGS += -O2 -Wall -std=c99
LDFLAGS += -ltiny3d -lrsx -lgcm_sys -lsysutil -lio -lm
all: $(TARGET).elf $(TARGET).self
$(TARGET).self: $(TARGET).elf
	$(SPRX) $(TARGET).elf $@
clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).self