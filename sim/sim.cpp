#include <isa_worker.hpp>
#include "jmpcoro_configured.hpp"
#include "or/or.hpp"
#include <ioiface.hpp>

int main(void)
{
	Thread main_thread;

	ISA_Pre_Init();

	optionrom_install(&main_thread);

	IoIface::ioiface_install();

	ISA_Init();

	ISA_Start();

    while(1)
    {
		main_thread.yield();
		optionrom_start_worker(&main_thread);
    }
}
