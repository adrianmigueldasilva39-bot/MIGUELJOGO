# Makefile
TARGET		:=	cobra
BUILD		:=	build
SOURCES		:=	.
DATA		:=	data
INCLUDES	:=	include
LIBDIR		:=

PS3DEV		?=	/opt/ps3dev
PSL1GHT		?=	/opt/ps3dev

TITLE		:=	Cobra Neon
APPID		:=	COBRANEON01
CONTENTID	:=	UP0001-COBRANEON01_00-0000000000000000

CFLAGS		:=	-O2 -Wall -mcpu=cell -mhard-float -fmodulo-sched -ffunction-sections -fdata-sections
CXXFLAGS	:=	$(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS		:=	$(CFLAGS)
LDFLAGS		:=	-Wl,--gc-sections
LIBS		:=	-lrsx -lgcm_sys -lsysutil -lio -lm

INCLUDE		:=	-I$(PSL1GHT)/ppu/include -I$(PS3DEV)/portlibs/ppu/include -I$(CURDIR)/$(INCLUDES)
LIBDIRS		:=	-L$(PSL1GHT)/ppu/lib -L$(PS3DEV)/portlibs/ppu/lib

include $(PSL1GHT)/ppu_rules

PARAM.SFO: sfo.xml
	@python3 $(PSL1GHT)/bin/sfo.py sfo.xml PARAM.SFO

pkg: $(TARGET).elf PARAM.SFO
	@$(PSL1GHT)/bin/make_self_npdrm $(TARGET).elf $(TARGET).self $(CONTENTID)
	@mkdir -p pkg/USRDIR
	@cp $(TARGET).self pkg/USRDIR/EBOOT.BIN
	@cp PARAM.SFO pkg/PARAM.SFO
	@cp -r $(DATA)/* pkg/USRDIR/ 2>/dev/null || true
	@$(PSL1GHT)/bin/make_package_npdrm pkg/CONFIG $(CONTENTID) $(TARGET).pkg 2>/dev/null || $(PSL1GHT)/bin/pkg.py --contentid $(CONTENTID) pkg/ $(TARGET).pkg 2>/dev/null || echo "PKG skip"
	@echo "OK: $(TARGET).pkg"

clean::
	@rm -rf $(TARGET).pkg $(TARGET).elf $(TARGET).self PARAM.SFO pkg $(BUILD) 2>/dev/null || true