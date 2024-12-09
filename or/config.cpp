#include "or_internal.hpp"
#include "psram_pio.hpp"
#include "config.hpp"
#include <snappy.h>
#include <snappy-sinksource.h>

static bool config_callback;

void config_install(Thread * main)
{

}

static bool config_decide(const ENTRY_STATE & state)
{
	if(state.entry == 1 && state.irq_no == 0x19)
		return true;
	if(config_callback && state.entry == 1 && state.irq_no == 0x13)
		return true;
	return false;
}

extern "C" const uint8_t _binary_bin_KERNEL_SYS_size[];
extern "C" const uint8_t _binary_bin_KERNEL_SYS_end[];
extern "C" const uint8_t _binary_bin_KERNEL_SYS_start[];

extern "C" const uint8_t _binary_config_img_spy_size[];
extern "C" const uint8_t _binary_config_img_spy_end[];
extern "C" const uint8_t _binary_config_img_spy_start[];

class PsramSink : public snappy::Sink{
	size_t off;
	Thread_SHM * thread;
public:
	PsramSink(Thread_SHM * _thread) : off(0), thread(_thread){};
	void Append(const char* data, size_t n) override
	{
		auto cpl = PSRAM::PSRAM::write_async_mem(off,reinterpret_cast<const uint8_t*>(data),n);
		while(!cpl.complete_trigger())
			thread->yield();
		off+=n;
	}

};

static void config_entry (Thread_SHM * thread)
{
	auto entry = thread->get_entry();
	if(entry.irq_no == 0x19)
	{
		thread->putstr("PicoPocket config: press C to ");
		thread->putstr("enter\r\n");
		config_callback = true;//(thread->getch() == 'c');
		if(!config_callback)
			thread->callback_end(); //nonreturn

		thread->install_irq(0x13);

		thread->putmem(0x60,0,const_cast<uint8_t*>(_binary_bin_KERNEL_SYS_start),
				_binary_bin_KERNEL_SYS_end - _binary_bin_KERNEL_SYS_start);

		thread->chain(0x00600000);

		auto params = thread->get_entry();
		params.regs.regs.bx = 0x0000;
		thread->set_return(params.regs);

		{
			snappy::ByteArraySource src = {
					reinterpret_cast<const char*>
					(_binary_config_img_spy_start),
					(size_t)
					(_binary_config_img_spy_end-_binary_config_img_spy_start)
			};
			PsramSink dst = {thread};
			snappy::Uncompress(&src,&dst);
		}

		thread->putstr("Config entered\r\n");
		thread->callback_end();
	}

	if(!ramdisk_handle(thread,0x00)) // don't chain only error
	{
		auto params = thread->get_entry();
		params.regs.regs.ax = 0x0101;
		params.regs.regs.rf |= 0x0001;
		params.regs.regs.rettype = 1;
		thread->set_return(params.regs);
	}


	thread->callback_end();
}

const OROMHandler config_handler = {
		config_decide,
		config_entry
};
