#include "or_internal.hpp"
#include "config.hpp"
#include <psram_pio.hpp>
#include <ar1021.hpp>


//8MB CHS
static constexpr size_t SECTORS = 32;
static constexpr size_t HEADS = 2;
static constexpr size_t CYLINDERS = 256;

void ramdisk_install(Thread * main)
{
	AR1021::AR1021::init();
	PSRAM::PSRAM::init();
}

static volatile uint32_t int13chain = 0;
static volatile uint8_t int13disk_no = 0;
static bool ramdisk_callback;

static bool ramdisk_decide(const ENTRY_STATE & state)
{
	if(state.entry == 1 && state.irq_no == 0x19)
		return true;
	if(ramdisk_callback && state.entry == 1 && state.irq_no == 0x13)
		return true;
	return false;
}

uint32_t params2lba(const ENTRY_STATE& params)
{
	uint16_t c = (params.regs.regs.cx>>8) | ((params.regs.regs.cx&0xC0)<<(8-6));
	uint8_t h = params.regs.regs.dx>>8;
	uint8_t s = params.regs.regs.cx & 0x3f;
	return (c*HEADS+h)*SECTORS+s-1;
}

struct [[gnu::packed]] DAP {
	uint8_t len;
	uint8_t u;
	uint16_t sectors;
	uint16_t off;
	uint16_t seg;
	uint32_t lbal;
};

static DAP get_dap(Thread_SHM * thread, const ENTRY_STATE& params)
{
	DAP ret;
	thread->getmem(params.regs.regs.ds,params.regs.regs.si,reinterpret_cast<uint8_t*>(&ret),sizeof(ret));
	return ret;
}

static void set_dap(Thread_SHM * thread, const ENTRY_STATE& params, DAP * dap)
{
	thread->putmem(params.regs.regs.ds,params.regs.regs.si,reinterpret_cast<uint8_t*>(dap),sizeof(*dap));
}

static uint8_t ramdisk_sector[512];

bool ramdisk_handle(Thread_SHM * thread, uint8_t drive_no)
{
	auto params = thread->get_entry();
	if ((params.regs.regs.dx & 0xff) != drive_no)
		return false;
	switch(params.regs.regs.ax>>8)
	{
	case 0x00: // reset
		params.regs.regs.ax = 0;
		params.regs.regs.rf &= ~0x0001;
		break;
	case 0x02: // read
	 {
		size_t sectors = params.regs.regs.ax & 0xff;
		uint32_t addr = params2lba(params)*512;
		uint16_t pcaddr = params.regs.regs.bx;
		for(size_t s=0;s<sectors;s++)
		{
			auto cpl = PSRAM::PSRAM::read_async_mem(addr,ramdisk_sector,512);
			while(!cpl.complete_trigger())
				thread->yield();
			thread->putmem(params.regs.regs.es,pcaddr,ramdisk_sector,512);
			addr+=512;
			pcaddr+=512;
		}
		params.regs.regs.ax = sectors | (0x00<<8);
		params.regs.regs.rf &= ~0x0001;
	 }
	 break;
	case 0x03: // write
	 {
		size_t sectors = params.regs.regs.ax & 0xff;
		uint32_t addr = params2lba(params)*512;
		uint16_t pcaddr = params.regs.regs.bx;
		for(size_t s=0;s<sectors;s++)
		{
			thread->getmem(params.regs.regs.es,pcaddr,ramdisk_sector,512);
			auto cpl = PSRAM::PSRAM::write_async_mem(addr,ramdisk_sector,512);
			while(!cpl.complete_trigger())
				thread->yield();
			addr+=512;
			pcaddr+=512;
		}
		params.regs.regs.ax = sectors | (0x00<<8);
		params.regs.regs.rf &= ~0x0001;
	 }
	 break;
	case 0x08: // get_chs
	 {
		params.regs.regs.bx = 0;
		uint8_t c = CYLINDERS-1;
		params.regs.regs.cx = ((c&0xff) <<8) | ((c&0x300)>>(8-6)) | SECTORS;
		params.regs.regs.dx = (HEADS-1 << 8) | 0x01;
		if(!(drive_no &0x80))
		{
			params.regs.regs.di = 0;
			params.regs.regs.es = 0;
		}
		params.regs.regs.ax &= 0x00ff;
		params.regs.regs.rf &= ~0x0001;
	 }
	 break;
	case 0x15: // dasd
	 {
		if(drive_no &0x80)
		{
			params.regs.regs.ax = 0x0003;
			uint32_t lba = CYLINDERS*HEADS*SECTORS;
			params.regs.regs.cx = lba>>16;
			params.regs.regs.dx = lba;
			params.regs.regs.rf &= ~0x0001;
		}
		else
		{
			params.regs.regs.ax &= 0x0001;
			params.regs.regs.rf &= ~0x0001;
		}
	 }
	 break;
	case 0x23: // set features
	case 0x24: // set multiple blocks
	case 0x25: // identify drive
	default:
		params.regs.regs.ax = 0x0101;
		params.regs.regs.rf |= 0x0001;
		break;
	case 0x01: // status
	case 0x09: // init disk
	case 0x0d: // areset
	case 0x0c: // seek
	case 0x47: // eseek
	case 0x10: // driveready
	case 0x11: // recalibrate
	case 0x14: // diagnostic
	case 0x04: // verify
	case 0x44: // ext_verify
		params.regs.regs.ax = 0x0000;
		params.regs.regs.rf &= ~0x0001;
		break;
	case 0x41: // ext_check
		params.regs.regs.bx = 0xAA55;
		params.regs.regs.cx = 0x01;
		params.regs.regs.ax = 0x2100 | (params.regs.regs.ax&0xFF);
		params.regs.regs.rf &= ~0x0001;
		break;
	case 0x42: // ext_read
	 {
		auto d = get_dap(thread,params);
		uint32_t addr = d.lbal*512;
		uint16_t pcaddr = d.off;
		for(size_t s=0;s<d.sectors;s++)
		{
			auto cpl = PSRAM::PSRAM::read_async_mem(addr,ramdisk_sector,512);
			while(!cpl.complete_trigger())
				thread->yield();
			thread->putmem(d.seg,pcaddr,ramdisk_sector,512);
			addr+=512;
			pcaddr+=512;
		}
		params.regs.regs.ax &= 0x00ff;
		params.regs.regs.rf &= ~0x0001;
	 }
	 break;
	case 0x43: // ext_write
	 {
		auto d = get_dap(thread,params);
		uint32_t addr = d.lbal*512;
		uint16_t pcaddr = d.off;
		for(size_t s=0;s<d.sectors;s++)
		{
			thread->getmem(d.seg,pcaddr,ramdisk_sector,512);
			auto cpl = PSRAM::PSRAM::write_async_mem(addr,ramdisk_sector,512);
			while(!cpl.complete_trigger())
				thread->yield();
			addr+=512;
			pcaddr+=512;
		}
		params.regs.regs.ax &= 0x00ff;
		params.regs.regs.rf &= ~0x0001;
	 }
	 break;
	case 0x48: // ext_params
	 {
		struct [[gnu::packed]] EDP {
			uint16_t len;
			uint16_t iflags;
			uint32_t cyls;
			uint32_t heads;
			uint32_t sectors_t;
			uint64_t lba;
			uint16_t bps;
			uint32_t edd;
		} edp = {
			0x1e,
			0,
			CYLINDERS,
			HEADS,
			SECTORS,
			CYLINDERS*HEADS*SECTORS,
			512,
			0
		};
		thread->putmem(params.regs.regs.ds,params.regs.regs.si,reinterpret_cast<uint8_t*>(&edp),sizeof(edp));
		params.regs.regs.ax &= 0x00ff;
		params.regs.regs.rf &= ~0x0001;
	 }
	 break;
	}
	params.regs.regs.rettype = 1;
	thread->set_return(params.regs);
	return true;
}

static void ramdisk_entry (Thread_SHM * thread)
{
	auto entry = thread->get_entry();
	if(entry.irq_no == 0x19)
	{
		if(entry.regs.regs.rettype == 0)
			thread->callback_end();
		bool autoboot = Config::RAMDISK_INSTALL::val.ival;
		thread->putstr("PicoPocket ramdisk: press R to ");
		if(autoboot)
			thread->putstr("stop ");
		thread->putstr("installing\r\n");
		ramdisk_callback = (thread->getch() == 'r') ^ autoboot;
		if(!ramdisk_callback)
			thread->callback_end(); //nonreturn

		uint8_t disks;
		thread->getmem(0,0x475,&disks,1);
		int13disk_no = 0x80 | disks;
		disks+=1;
		thread->putmem(0,0x475,&disks,1);

		int13chain = thread->install_irq(0x13);

		thread->putstr("Ramdisk installed\r\n");
		thread->callback_end();
	}

	if(int13chain)
		thread->chain(int13chain);


	ramdisk_handle(thread,int13disk_no);


	thread->callback_end();
}

const OROMHandler ramdisk_handler = {
		ramdisk_decide,
		ramdisk_entry
};
