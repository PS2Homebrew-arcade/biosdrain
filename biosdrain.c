#include "config.h"
#include "ui/menu.h"
#include "ui/fontqueue.h"
#include "ui/fontengine.h"
#include "dump.h"

#include <kernel.h>
#include <stdio.h>
#include <sio.h>
#include <debug.h>
#include <stdlib.h>
#include <dirent.h> // mkdir()
#include <unistd.h> // rmdir()
#include <graph.h> // graph_wait_vsync()
#include <sbv_patches.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <iopcontrol.h>

#include "sysman/sysinfo.h"
#include "sysman_rpc.h"

#include "OSDInit.h"

#define BIN2C(_irx) \
extern unsigned int size_##_irx; \
extern unsigned char _irx[]

BIN2C(bdm_irx);
BIN2C(bdmfs_fatfs_irx);
BIN2C(usbmass_bd_irx);
BIN2C(usbd_irx);
BIN2C(sysman_irx);
BIN2C(mmceman_irx);


void sysman_prerequesites()
{
	// does not exist on COH-H
	// SifLoadModule("rom0:ADDDRV", 0, NULL);
	// SifLoadModule("rom0:ADDROM2", 0, NULL);
}

t_SysmanHardwareInfo g_hardwareInfo;
void LoadSystemInformation()
{
	SysmanGetHardwareInfo(&g_hardwareInfo);
	scr_setfontcolor(0x00FFFF);
	if (g_hardwareInfo.ROMs[0].IsExists)
		scr_printf("- ROM0 ADDR and SIZE: %08X %08X\n", g_hardwareInfo.ROMs[0].StartAddress, g_hardwareInfo.ROMs[0].size);
	else
		scr_printf("- WTF: ROM0 not detected\n");
	scr_setfontcolor(0xFFFFFF);

	if (g_hardwareInfo.DVD_ROM.IsExists)
		scr_printf("- DVD ADDR and SIZE: %08X %08X\n", g_hardwareInfo.DVD_ROM.StartAddress, g_hardwareInfo.DVD_ROM.size);

	if (g_hardwareInfo.ROMs[1].IsExists)
	{
		scr_setfontcolor(0x00FFFF);
		scr_printf("  UNICORN!!! DEVELOPER FLASH DETECTED!!!\n");
		scr_printf("  PLEASE CONTACT El_isra at github.com/PS2Homebrew-arcade/biosdrain/issues/new");
		scr_setfontcolor(0xFFFFFF);
		scr_printf(" - ROM1 ADDR and SIZE: %08X %08X\n", g_hardwareInfo.ROMs[1].StartAddress, g_hardwareInfo.ROMs[1].size);
	}
	if (g_hardwareInfo.ROMs[2].IsExists)
		scr_printf(" - ROM2 ADDR and SIZE: %08X %08X\n", g_hardwareInfo.ROMs[2].StartAddress, g_hardwareInfo.ROMs[2].size);

	scr_printf("- BoardInf: %02X\n",g_hardwareInfo.BoardInf);
	
	scr_printf("- ILINK: ports:%d maxspeed:%02X compliance:%02X Vendor:%08X Product:%08X\n", 
		g_hardwareInfo.iLink.NumPorts,
		g_hardwareInfo.iLink.MaxSpeed,
		g_hardwareInfo.iLink.ComplianceLevel,
		g_hardwareInfo.iLink.VendorID,
		g_hardwareInfo.iLink.ProductID);
	scr_printf("- SPU: Revision %04X\n", g_hardwareInfo.spu2.revision);
	scr_printf("- MPU: BoardID  %04X\n", g_hardwareInfo.MPUBoardID);
	scr_printf("- IOP: Rev:%04X, RamSize:%08X\n", g_hardwareInfo.iop.revision, g_hardwareInfo.iop.RAMSize);
	u16 EEREV = GetCop0(15);
	scr_printf("- EE : Rev:%04X, Impl:%08X\n", EEREV >> 8, EEREV & 0xFF);

}

void load_irx_usb()
{
	int bdm_irx_id = SifExecModuleBuffer(&bdm_irx, size_bdm_irx, 0, NULL, NULL);
	printf("BDM ID: %d\n", bdm_irx_id);

	int bdmfs_fatfs_irx_id = SifExecModuleBuffer(&bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, NULL, NULL);
	printf("BDMFS FATFS ID: %d\n", bdmfs_fatfs_irx_id);

	int usbd_irx_id = SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, NULL);
	printf("USBD ID is %d\n", usbd_irx_id);

	int usbmass_irx_id = SifExecModuleBuffer(&usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL, NULL);
	printf("USB Mass ID is %d\n", usbmass_irx_id);

	int mmceman_irx_id = SifExecModuleBuffer(&mmceman_irx, size_mmceman_irx, 0, NULL, NULL);
	printf("MMCEMAN ID is %d\n", mmceman_irx_id);
};

int wait_dev_ready(const char* dev)
{
    struct stat buffer;
    int ret = -1;
    int retries = 50;

    while(ret != 0 && retries > 0)
    {
        ret = stat(dev, &buffer);
        /* Wait until the device is ready */
        nopdelay();

        retries--;
    }
	return(retries != 0);
}

void reset_iop()
{
	SifInitRpc(0);
	// Reset IOP
	while (!SifIopReset("", 0x0))
		;
	while (!SifIopSync())
		;
	SifInitRpc(0);

	sbv_patch_enable_lmb();
}
enum TARGETS {NONE=0, USB=1, MMCE0=2, MMCE1=3};
u32 final_target = 0;
u32 use_usb_dir = 0;
u32 use_mmce0 = 0;
u32 use_mmce1 = 0;
int determine_device()
{
	load_irx_usb();
#ifndef FORCE_USB
	// Check host first, can't open just host: on pcsx2, it's broken.
	if (!mkdir("host:tmp", 0777))
	{
		rmdir("host:tmp");
		use_usb_dir = 0;
	}
	else
#else
	scr_printf("Built to force USB and skip HOST.\n");
#endif
	{

		use_usb_dir = !wait_dev_ready("usb:/");
		use_mmce0   = !wait_dev_ready("mmce0:/");
		use_mmce1   = !wait_dev_ready("mmce1:/");
		if (use_usb_dir) final_target = USB;
		if (use_mmce1) final_target = MMCE1;
		if (use_mmce0) final_target = MMCE0;

		if(final_target == NONE)
		{
			scr_setfontcolor(0x00FFFF);
			scr_printf("  usable USB/MMCE device for storing dump not found.. not continuing.\n");
			scr_setfontcolor(0xFFFFFF);
			return 1;
		}
	}
	return 0;
}

void load_irx_sysman()
{
	sbv_patch_enable_lmb();
	sysman_prerequesites();
	int mod_res;
	int sysman_irx_id = SifExecModuleBuffer(&sysman_irx, size_sysman_irx, 0, NULL, &mod_res);
	printf("SYSMAN ID is %d (res %d)\n ", sysman_irx_id, mod_res);

	SysmanInit();
}

int main(void)
{
	sio_puts("main()\n");

	init_scr();

	scr_printf("\n\n\n biosdrain for SYSTEM2x6 - Modified By El_isra - revision %s\n", GIT_VERSION);
	if (determine_device())
		goto exit_main;

	load_irx_sysman();

	char* targets[4] = {"host:", "usb:", "mmce0:", "mmce1:"}; 
	scr_printf("Dumping to %s\n", targets[final_target]);

	LoadSystemInformation();

	dump_init(final_target);

	dump_exec();
	dump_cleanup();
exit_main:
	scr_printf("Finished everything. You're free to turn off the system.\n");
	scr_printf("\n");
	SleepThread();
}

void _ps2sdk_memory_init() {
	reset_iop();
	SifLoadModule("rom0:CDVDFSV", 0, NULL); // not included on stock IOPBTCONF
	SifLoadModule("rom0:SIO2MAN", 0, NULL);
	SifLoadModule("rom0:MCMAN", 0, NULL);
	SifLoadModule("rom0:DAEMON", 0, NULL); // needed to avoid console from rebooting itself mid-dump
}