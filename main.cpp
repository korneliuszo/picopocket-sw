
#define _HARDWARE_UART_H
#define _PICO_STDIO_UART_H

#include <string.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/vreg.h"
#include "hardware/regs/vreg_and_chip_reset.h"
#include "hardware/regs/busctrl.h"              /* Bus Priority defines */
#include "hardware/clocks.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/clocks.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "network_l2.hpp"
#include "www.hpp"
#include "config.hpp"
#include <isa_worker.hpp>
#include "or/or.hpp"
#include <ioiface.hpp>
#include <config_iface.hpp>
#include "16550/uart_tcp_server.hpp"
#include "audio/audio.hpp"
#include "audio_dma.hpp"

constexpr uint32_t PICO_Freq=250; //PM_SYS_CLK;

static void copy_optionroms(void) {
	extern char __core1_static_start__[];
	extern char __core1_static_end__[];
	extern char __core1_static_source__[];

	memcpy(__core1_static_start__, __core1_static_source__, (__core1_static_end__-__core1_static_start__));
}

LWIP_TCP_16550 uart1;

extern "C" volatile uint32_t __main_loop_us__;
extern "C" volatile uint32_t __dma_isr_avail__;

int main(void)
{
	Thread main_thread;
	bool uart1_enabled=false;
	// Overclock!
	vreg_set_voltage(VREG_VOLTAGE_1_25);
	sleep_ms(100);
	set_sys_clock_khz(PICO_Freq*1000, true);

	ISA_Pre_Init();

	Config::Flash_Storage::load();

	copy_optionroms();
	network_init();

	optionrom_install(&main_thread);

	IoIface::ioiface_install();
	install_config_iface();
	if(Config::UART1_ADDR::val.ival)
	{
		uart1_enabled = true;
		uart1.connect(
			Config::UART1_ADDR::val.ival,
			Config::UART1_IRQ_NOT_MACRO::val.ival,
			Config::UART1_PORT::val.ival);
	}
	sbdsp_install(&main_thread);
	mss_install(&main_thread);

	ISA_Init();

	tusb_init();

	www_init();

	bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC1_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS | BUSCTRL_BUS_PRIORITY_DMA_W_BITS;
	ISA_Start();

	while(1)
	{
		uint32_t start = time_us_32();
		tud_task(); // tinyusb device task
		main_thread.yield();
		network_poll();
		main_thread.yield();
		optionrom_start_worker(&main_thread);
		if(uart1_enabled)
		{
			main_thread.yield();
			uart1.poll();
		}
		main_thread.yield();
		sbdsp_poll(&main_thread);
		main_thread.yield();
		mss_poll(&main_thread);
		main_thread.yield();
		__main_loop_us__ = time_us_32() - start;
		__dma_isr_avail__ = AudioDMA::AudioDMA::isr_time_taken;
	}

	return 0;
}

// Implement callback to add our custom driver
usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count) {

    extern usbd_class_driver_t __tusb_driver_class_start;
    extern usbd_class_driver_t __tusb_driver_class_end;

	*driver_count = &__tusb_driver_class_end - &__tusb_driver_class_start;
	return &__tusb_driver_class_start;
}
