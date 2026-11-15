# Makefile - Cobra Neon PS3 (PSL1GHT)
# Sem tiny3d, so RSX + GCM padrao

TARGET		:=	cobra
BUILD		:=	build
SOURCES		:=	.
DATA		:=	data
INCLUDES	:=	include
LIBDIR		:=

# Se o PS3DEV nao estiver setado, usa o padrao do teu CI
PS3DEV		?=	/opt/ps3dev
PSL1GHT		?=	/opt/ps3dev

TITLE		:=	Cobra Neon
APPID		:=	COBRANEON01
CONTENTID	:=	UP0001-COBRANEON01_00-0000000000000000

# Flags
CFLAGS		:=	-O2 -Wall -mcpu=cell -mhard-float -fmodulo-sched \
			-ffunction-sections -fdata-sections
CXXFLAGS	:=	$(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS		:=	$(CFLAGS)
LDFLAGS		:=	-Wl,--gc-sections

# Libs - ORDEM IMPORTA, nao mexe
LIBS		:=	-lrsx -lgcm_sys -lsysutil -lio -lm

# Includes padrao do PSL1GHT
INCLUDE		:=	-I$(PSL1GHT)/ppu/include -I$(PS3DEV)/portlibs/ppu/include \
			-I$(CURDIR)/$(INCLUDES)

LIBDIRS		:=	-L$(PSL1GHT)/ppu/lib -L$(PS3DEV)/portlibs/ppu/lib

# Regras do PSL1GHT (tem que vir DEPOIS das variaveis)
include $(PSL1GHT)/ppu_rules

# Limpa PKG tambem
clean::
	@rm -rf $(TARGET).pkg $(TARGET).elf $(BUILD) 2>/dev/null || true