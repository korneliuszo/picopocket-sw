#include "or_internal.hpp"

extern const
OROMHandler int19_handler;

void int19_install(Thread * main)
{

}

static volatile uint32_t int19chain = 0;

static bool int19_decide(const ENTRY_STATE & state)
{
	if(state.entry == 0) //boot
		return true;
	if(int19chain && state.entry == 1 && state.irq_no == 0x19)
		return true;
	return false;
}

static void int19_entry (Thread_SHM * thread)
{
	if(thread->entry.entry == 0)
	{
		int19chain = thread->install_irq(0x19);
		static const char str[] = "PicoPocket int19 installed\r\n";
		thread->putstr(str);
		thread->callback_end();
	}
	if(int19chain && thread->entry.regs.regs.rettype == 0x01)
	{
		thread->chain(int19chain);
		thread->set_return();
		static const char str[] = "PicoPocket int19 chained\r\n";
		thread->putstr(str);
	}
	thread->callback_end();
}

OROMHandler_type_section
OROMHandler int19_handler = {
		int19_decide,
		int19_entry
};
