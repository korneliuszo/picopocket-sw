#include <isa_worker.hpp>
#include "jmpcoro_configured.hpp"
#include "or/or.hpp"
#include <ioiface.hpp>
#include <config_iface.hpp>

int main(void)
{
	Thread main_thread;

	ISA_Pre_Init();

	optionrom_install(&main_thread);

	IoIface::ioiface_install();
	install_config_iface();

	ISA_Init();

	ISA_Start();

    while(1)
    {
		main_thread.yield();
		optionrom_start_worker(&main_thread);
    }
}
