#include "or_internal.hpp"
#include "config.hpp"
#include "psram_pio.hpp"
#include "ar1021.hpp"
#include "int13_handler.hpp"

//8MB CHS
static constexpr size_t SECTORS = 32;
static constexpr size_t HEADS = 2;
static constexpr size_t CYLINDERS = 256;

extern "C" const uint8_t _binary_mbr_bin_size[];
extern "C" const uint8_t _binary_mbr_bin_end[];
extern "C" const uint8_t _binary_mbr_bin_start[];


void ramdisk_install(Thread * main)
{
	AR1021::AR1021::init();
	PSRAM::PSRAM::init();
	const uint8_t * data = _binary_mbr_bin_start;
	const ssize_t n = _binary_mbr_bin_end-_binary_mbr_bin_start;

	auto cpl = PSRAM::PSRAM::write_async_mem(0,data,n);
	while(!cpl.complete_trigger())
		main->yield();
}

static volatile uint32_t int13chain = 0;
static volatile uint8_t int13disk_no = 0;
static bool ramdisk_callback;

static bool ramdisk_decide(const volatile ENTRY_STATE & state)
{
	if(state.entry == 1 && state.irq_no == 0x19)
		return true;
	if(ramdisk_callback && state.entry == 1 && state.irq_no == 0x13)
		return true;
	if(state.entry == 2)
		return true;
	return false;
}


static void ramdisk_entry (Thread_SHM * thread)
{
	if(thread->params.irq_no == 0x19 || thread->params.entry == 2)
	{
		if(thread->params.regs.regs.rettype&0x80)
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

	int13_handle<ramdisk_read,ramdisk_write>(thread,int13disk_no,
			CYLINDERS,HEADS,SECTORS,CYLINDERS*HEADS*SECTORS);

	thread->callback_end();
}

const OROMHandler ramdisk_handler = {
		ramdisk_decide,
		ramdisk_entry
};
