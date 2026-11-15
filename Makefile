TARGET = slither
OBJS = main.o
PSL1GHT = $(shell psl1ght-config --prefix)
include $(PSL1GHT)/ppu_rules
CFLAGS += -O2 -Wall -std=c99
LDFLAGS += -ltiny3d -lrsx -lgcm_sys -lsysutil -lio -lm
all: $(TARGET).elf $(TARGET).self
$(TARGET).self: $(TARGET).elf
	$(SPRX) $(TARGET).elf $@
clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).self