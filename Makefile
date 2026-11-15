ifeq ($(strip $(PSL1GHT)),)
$(error "Defina a variavel PSL1GHT")
endif
include $(PSL1GHT)/ppu_rules

.DEFAULT_GOAL := all

TARGET    := cobra
TITLE     := Cobra Neon
APPID     := COBR00001
CONTENTID := UP0001-$(APPID)_00-0000000000000000
SFOXML    := sfo.xml
PORTLIBS  := $(PS3DEV)/portlibs/ppu

MACHDEP := -mhard-float -fmodulo-sched -ffunction-sections -fdata-sections
CFLAGS  := -O2 -Wall -mcpu=cell $(MACHDEP) -I$(PSL1GHT)/ppu/include -I$(PORTLIBS)/include
LDFLAGS := $(MACHDEP) -L$(PSL1GHT)/ppu/lib -L$(PORTLIBS)/lib
LIBS    := -lrsx -lgcm_sys -lio -lsysutil -lrt -llv2 -lm

all: $(TARGET).pkg

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: main.o
	$(CC) $(LDFLAGS) -o $@ $< $(LIBS)

ICON0.PNG:
	python3 -c "import zlib,struct;w,h=320,176;raw=b''.join(b'\x00'+b'\x05\x05\x12\xff'*w for _ in range(h));ihdr=struct.pack('>IIBBBBB',w,h,8,6,0,0,0);c1=b'IHDR'+ihdr;c2=b'IDAT'+zlib.compress(raw);c3=b'IEND';png=b'\x89PNG\r\n\x1a\n'+struct.pack('>I',13)+c1+struct.pack('>I',zlib.crc32(c1))+struct.pack('>I',len(c2)-4)+c2+struct.pack('>I',zlib.crc32(c2))+b'\x00\x00\x00\x00'+c3+struct.pack('>I',zlib.crc32(c3));open('ICON0.PNG','wb').write(png)"

$(SFOXML):
	cp $(PSL1GHT)/bin/sfo.xml $(SFOXML) 2>/dev/null || printf '<?xml version="1.0" encoding="utf-8"?>\n<sfo>\n  <value name="ATTRIBUTE" type="integer">32</value>\n  <value name="BOOTABLE" type="integer">1</value>\n  <value name="CATEGORY" type="string">HG</value>\n  <value name="PARENTAL_LEVEL" type="integer">0</value>\n  <value name="PS3_SYSTEM_VER" type="string">03.4000</value>\n  <value name="RESOLUTION" type="integer">63</value>\n  <value name="SOUND_FORMAT" type="integer">1</value>\n  <value name="VERSION" type="string">01.00</value>\n</sfo>\n' > $(SFOXML)

$(TARGET).pkg: $(TARGET).elf ICON0.PNG $(SFOXML)
	rm -rf pkg
	mkdir -p pkg/USRDIR
	cp ICON0.PNG pkg/ICON0.PNG
	$(PS3DEV)/bin/sprxlinker $(TARGET).elf
	$(PS3DEV)/bin/make_self_npdrm $(TARGET).elf pkg/USRDIR/EBOOT.BIN $(CONTENTID)
	$(PS3DEV)/bin/sfo.py --title "$(TITLE)" --appid "$(APPID)" -f $(SFOXML) pkg/PARAM.SFO
	$(PS3DEV)/bin/pkg.py --contentid $(CONTENTID) pkg/ $@

clean:
	rm -rf pkg *.o *.elf *.self *.pkg ICON0.PNG $(SFOXML)
