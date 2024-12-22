#include <isa_worker.hpp>
#include "jmpcoro_configured.hpp"
#include "or/or.hpp"
#include <ioiface.hpp>
#include <config_iface.hpp>
#include "16550/uart_tcp_server.hpp"

LWIP_TCP_16550 uart1;

int main(void)
{
	Thread main_thread;

	ISA_Pre_Init();

	optionrom_install(&main_thread);

	IoIface::ioiface_install();
	install_config_iface();
	uart1.connect(0x3F8,4,5556);

	ISA_Init();

	ISA_Start();

    while(1)
    {
		main_thread.yield();
		optionrom_start_worker(&main_thread);
		uart1.poll();
    }
}
