EE_BIN = bin/biosdrain_arcade.elf
EE_OBJS = biosdrain.o OSDInit.o sysman_rpc.o dump.o modelname.o
IRX_OBJS = $(addprefix irx/, $(addsuffix _irx.o, usbmass_bd usbd bdm bdmfs_fatfs sysman mmceman fileXio iomanX))
# Bin2c objects that will be linked in
EE_OBJS += $(IRX_OBJS)
EE_LIBS = -lkernel -lpatches -ldebug -lgraph -ldma -ldraw -lfileXio

EE_DVP = dvp-as

# Git version
GIT_VERSION := "$(shell git describe --abbrev=4 --always --tags)"

EE_CFLAGS = -I$(shell pwd) -Werror -DGIT_VERSION="\"$(GIT_VERSION)\""

IRX_C_FILES = usbmass_bd_irx.c bdm_irx.c bdmfs_fatfs_irx.c usbd_irx.c sysman_irx.c

all: sysman_irx $(EE_BIN)

# IRX files to be built and or bin2c'd
sysman_irx:
	$(MAKE) -C sysman

irx/sysman_irx.c: sysman/sysman.irx
	bin2c $< irx/sysman_irx.c sysman_irx

vpath %.irx irx/
vpath %.irx $(PS2SDK)/iop/irx

IRXTAG = $(notdir $(addsuffix _irx, $(basename $<)))
irx/%_irx.c: %.irx
	bin2c $< $@ $(IRXTAG)

clean:
	$(MAKE) -C sysman clean
	rm -f $(EE_OBJS) $(IRX_C_FILES)

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

wsl: $(EE_BIN)
	$(PCSX2) --elf="$(shell wslpath -w $(shell pwd))/$(EE_BIN)"

emu: $(EE_BIN)
	$(PCSX2) --elf="$(shell pwd)/$(EE_BIN)"

reset:
	ps2client reset
	ps2client netdump

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
