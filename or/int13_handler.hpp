#pragma once
#include "or_internal.hpp"
#include "psram_pio.hpp"

static uint32_t params2lba(const ENTRY_STATE& params,
		uint16_t C,
		uint8_t H,
		uint8_t S
		)
{
	uint16_t c = (params.regs.regs.cx>>8) | ((params.regs.regs.cx&0xC0)<<(8-6));
	uint8_t h = params.regs.regs.dx>>8;
	uint8_t s = params.regs.regs.cx & 0x3f;
	return (c*H+h)*S+s-1;
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

static uint8_t ramdisk_sector[2][512];

inline auto ramdisk_read(Thread_SHM * thread, uint8_t drive_no, uint32_t LBA,uint8_t sector[512])
{
	return PSRAM::PSRAM::read_async_mem(LBA*512,sector,512);
}

inline auto ramdisk_write(Thread_SHM * thread, uint8_t drive_no, uint32_t LBA,uint8_t sector[512])
{
	return PSRAM::PSRAM::write_async_mem(LBA*512,sector,512);
}


template<auto sread, auto swrite>
inline bool int13_handle(Thread_SHM * thread, uint8_t drive_no,
		uint16_t C,
		uint8_t H,
		uint8_t S,
		uint32_t LBA)
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
		uint32_t addr = params2lba(params,C,H,S);
		uint16_t pcaddr = params.regs.regs.bx;
		if(sectors)
		{
			auto cpl = sread(thread,drive_no,addr,ramdisk_sector[0]);
			for(size_t s=0;s<sectors;s+=2)
			{
				while(!cpl.complete_trigger())
					thread->yield();
				if(s+1<sectors)
					cpl = sread(thread,drive_no,addr+1,ramdisk_sector[1]);
				thread->putmem(params.regs.regs.es,pcaddr,ramdisk_sector[0],512);
				if(s+1<sectors)
				{
					while(!cpl.complete_trigger())
						thread->yield();
					addr+=2;
					if(s+2<sectors)
						cpl = sread(thread,drive_no,addr,ramdisk_sector[0]);
					thread->putmem(params.regs.regs.es,pcaddr+512,ramdisk_sector[1],512);
					pcaddr+=1024;
				}
			}
		}
		params.regs.regs.ax = sectors | (0x00<<8);
		params.regs.regs.rf &= ~0x0001;
	 }
	 break;
	case 0x03: // write
	 {
		size_t sectors = params.regs.regs.ax & 0xff;
		uint32_t addr = params2lba(params,C,H,S)*1;
		uint16_t pcaddr = params.regs.regs.bx;
		if(sectors)
		{
			thread->getmem(params.regs.regs.es,pcaddr,ramdisk_sector[0],512);
			for(size_t s=0;s<sectors;s+=2)
			{
				auto cpl = swrite(thread,drive_no,addr,ramdisk_sector[0]);
				addr+=1;
				pcaddr+=512;
				if(s+1<sectors)
					thread->getmem(params.regs.regs.es,pcaddr+512,ramdisk_sector[1],512);
				while(!cpl.complete_trigger())
					thread->yield();
				if(s+1<sectors)
				{
					cpl = swrite(thread,drive_no,addr+1,ramdisk_sector[1]);
					addr+=2;
					pcaddr+=1024;
					if(s+2<sectors)
						thread->getmem(params.regs.regs.es,pcaddr,ramdisk_sector[0],512);
					while(!cpl.complete_trigger())
						thread->yield();
				}
			}
		}
		params.regs.regs.ax = sectors | (0x00<<8);
		params.regs.regs.rf &= ~0x0001;
	 }
	 break;
	case 0x08: // get_chs
	 {
		params.regs.regs.bx = 0;
		uint8_t c = C-1;
		params.regs.regs.cx = ((c&0xff) <<8) | ((c&0x300)>>(8-6)) | S;

		uint8_t disks;
		thread->getmem(0,0x475,&disks,1);

		params.regs.regs.dx = (H-1 << 8) | disks;
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
			uint32_t lba = LBA;
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
		uint32_t addr = d.lbal;
		uint16_t pcaddr = d.off;
		if(d.sectors)
		{
			auto cpl = sread(thread,drive_no,addr,ramdisk_sector[0]);
			for(size_t s=0;s<d.sectors;s+=2)
			{
				while(!cpl.complete_trigger())
					thread->yield();
				if(s+1<d.sectors)
					cpl = sread(thread,drive_no,addr+1,ramdisk_sector[1]);
				thread->putmem(params.regs.regs.es,pcaddr,ramdisk_sector[0],512);
				if(s+1<d.sectors)
				{
					while(!cpl.complete_trigger())
						thread->yield();
					addr+=2;
					if(s+2<d.sectors)
						cpl = sread(thread,drive_no,addr,ramdisk_sector[0]);
					thread->putmem(params.regs.regs.es,pcaddr+512,ramdisk_sector[1],512);
					pcaddr+=1024;
				}
			}
		}
		params.regs.regs.ax &= 0x00ff;
		params.regs.regs.rf &= ~0x0001;
	 }
	 break;
	case 0x43: // ext_write
	 {
		auto d = get_dap(thread,params);
		uint32_t addr = d.lbal*1;
		uint16_t pcaddr = d.off;
		if(d.sectors)
		{
			thread->getmem(params.regs.regs.es,pcaddr,ramdisk_sector[0],512);
			for(size_t s=0;s<d.sectors;s+=2)
			{
				auto cpl = swrite(thread,drive_no,addr,ramdisk_sector[0]);
				addr+=1;
				pcaddr+=512;
				if(s+1<d.sectors)
					thread->getmem(params.regs.regs.es,pcaddr+512,ramdisk_sector[1],512);
				while(!cpl.complete_trigger())
					thread->yield();
				if(s+1<d.sectors)
				{
					cpl = swrite(thread,drive_no,addr+1,ramdisk_sector[1]);
					addr+=2;
					pcaddr+=1024;
					if(s+2<d.sectors)
						thread->getmem(params.regs.regs.es,pcaddr,ramdisk_sector[0],512);
					while(!cpl.complete_trigger())
						thread->yield();
				}
			}
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
			C,
			H,
			S,
			LBA,
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
