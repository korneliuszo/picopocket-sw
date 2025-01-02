#include "../common/config.hpp"

#include "or_internal.hpp"
#include "psram_pio.hpp"
#include "int13_handler.hpp"

static volatile bool config_callback;

void config_install(Thread * main)
{

}

static bool config_decide(const volatile ENTRY_STATE & state)
{
	if(state.entry == 1 && state.irq_no == 0x19)
		return true;
	if(state.entry == 2)
		return true;
	if(config_callback && state.entry == 1 && state.irq_no == 0x13)
		return true;
	return false;
}

extern "C" const uint8_t _binary_bin_KERNEL_SYS_size[];
extern "C" const uint8_t _binary_bin_KERNEL_SYS_end[];
extern "C" const uint8_t _binary_bin_KERNEL_SYS_start[];

extern "C" const uint8_t _binary_config_img_size[];
extern "C" const uint8_t _binary_config_img_end[];
extern "C" const uint8_t _binary_config_img_start[];

static void config_entry (Thread_SHM * thread)
{
	if(thread->params.irq_no == 0x19 || thread->params.entry == 2)
	{
		if(thread->params.regs.regs.rettype&0x80)
			thread->callback_end();
		thread->putstr("PicoPocket config: press C to ");
		thread->putstr("enter\r\n");
		config_callback = (thread->getch() == 'c');
		if(!config_callback)
			thread->callback_end(); //nonreturn

		thread->install_irq(0x13);

		uint8_t disks;
		disks=0;
		thread->putmem(0,0x475,&disks,1);

		thread->putmem(0x60,0,const_cast<uint8_t*>(_binary_bin_KERNEL_SYS_start),
				_binary_bin_KERNEL_SYS_end - _binary_bin_KERNEL_SYS_start);

		thread->chain(0x00600000);

		thread->params.regs.regs.bx = 0x0000;
		thread->params.regs.regs.rettype = 0x80;

		{
			const uint8_t * data = _binary_config_img_start;
			const ssize_t n = _binary_config_img_end-_binary_config_img_start;

			auto cpl = PSRAM::PSRAM::Write_Mem_Task(0,data,n);
			while(!cpl.complete_trigger())
				thread->yield();
		}

		thread->putstr("Config entered\r\n");
		thread->callback_end();
	}

	if(!int13_handle<ramdisk_read,ramdisk_write>(thread,0x00,
					32,2,32,32*2*32)) // don't chain only error
	{
		thread->params.regs.regs.ax = 0x0101;
		thread->params.regs.regs.rf |= 0x0001;
		thread->params.regs.regs.rettype = 1;
	}


	thread->callback_end();
}

const OROMHandler config_handler = {
		config_decide,
		config_entry
};
