# Makefile - Cobra Neon PS3 (PSL1GHT)
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
LDFLAGS		:=	-Wl,--gc-sections

LIBS		:=	-lrsx -lgcm_sys -lsysutil -lio -lm

# Detecta onde o PSL1GHT esta instalado (raiz ou subpasta psl1ght)
ifneq ("$(wildcard $(PSL1GHT)/ppu_rules)","")
	PSL1GHT_DIR := $(PSL1GHT)
else
	PSL1GHT_DIR := $(PSL1GHT)/psl1ght
endif

INCLUDE		:=	-I$(PSL1GHT_DIR)/ppu/include -I$(PS3DEV)/portlibs/ppu/include \
			-I$(CURDIR)/$(INCLUDES)

LIBDIRS		:=	-L$(PSL1GHT_DIR)/ppu/lib -L$(PS3DEV)/portlibs/ppu/lib

# Garante que pastas e ICON0.PNG existam antes de empacotar
$(shell mkdir -p $(DATA) $(INCLUDES))
$(shell [ -f ICON0.PNG ] || python3 -c "import zlib,struct;w,h=320,176;raw=b''.join(b'\x00'+b'\x05\x05\x12\xff'*w for _ in range(h));ihdr=struct.pack('>IIBBBBB',w,h,8,6,0,0,0);png=b'\x89PNG\r\n\x1a\n'+struct.pack('>I',13)+b'IHDR'+ihdr+struct.pack('>I',zlib.crc32(b'IHDR'+ihdr))+struct.pack('>I',len(zlib.compress(raw)))+b'IDAT'+zlib.compress(raw)+struct.pack('>I',zlib.crc32(b'IDAT'+zlib.compress(raw)))+b'\x00\x00\x00\x00IEND\xaeB`\x82';open('ICON0.PNG','wb').write(png)")

include $(PSL1GHT_DIR)/ppu_rules

clean::
	@rm -rf $(TARGET).pkg $(TARGET).elf $(TARGET).self PARAM.SFO ICON0.PNG $(BUILD) 2>/dev/null || true
