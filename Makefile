TARGET		:=	cobra
BUILD		:=	build
SOURCES		:=	.
DATA		:=	data
INCLUDES	:=	include
LIBDIR		:=

PS3DEV		?=	/usr/local/ps3dev
PSL1GHT		?=	$(PS3DEV)

TITLE		:=	Cobra Neon
APPID		:=	COBRANEON01
CONTENTID	:=	UP0001-COBRANEON01_00-0000000000000000

CFLAGS		:=	-O2 -Wall -mcpu=cell -mhard-float -ffunction-sections -fdata-sections
CXXFLAGS	:=	$(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS		:=	$(CFLAGS)
LDFLAGS		:=	--gc-sections

LIBS		:=	-lrsx -lgcm_sys -lsysutil -lio -lm

ifneq ("$(wildcard $(PSL1GHT)/ppu_rules)","")
	PSL1GHT_DIR := $(PSL1GHT)
else
	PSL1GHT_DIR := $(PSL1GHT)/psl1ght
endif

INCLUDE		:=	-I$(PSL1GHT_DIR)/ppu/include -I$(PS3DEV)/portlibs/ppu/include \
			-I$(CURDIR)/$(INCLUDES)

LIBDIRS		:=	-L$(PSL1GHT_DIR)/ppu/lib -L$(PS3DEV)/portlibs/ppu/lib

$(shell mkdir -p $(DATA) $(INCLUDES))

include $(PSL1GHT_DIR)/ppu_rules

pkg: $(TARGET).self
	@mkdir -p pkg/USRDIR
	@cp $(TARGET).self pkg/USRDIR/EBOOT.BIN
	@cp PARAM.SFO pkg/PARAM.SFO
	@python3 -c "import zlib,struct;w,h=320,176;raw=b''.join(b'\x00'+b'\x05\x05\x12\xff'*w for _ in range(h));ihdr=struct.pack('>IIBBBBB',w,h,8,6,0,0,0);c1=b'IHDR'+ihdr;c2=b'IDAT'+zlib.compress(raw);c3=b'IEND';png=b'\x89PNG\r\n\x1a\n'+struct.pack('>I',13)+c1+struct.pack('>I',zlib.crc32(c1))+struct.pack('>I',len(c2)-4)+c2+struct.pack('>I',zlib.crc32(c2))+b'\x00\x00\x00\x00'+c3+struct.pack('>I',zlib.crc32(c3));open('pkg/ICON0.PNG','wb').write(png)"
	@pkg.py --contentid $(CONTENTID) pkg/ $(TARGET).pkg

clean::
	@rm -rf $(TARGET).pkg $(TARGET).elf $(TARGET).self PARAM.SFO ICON0.PNG pkg $(BUILD) 2>/dev/null || true
