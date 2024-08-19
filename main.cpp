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

constexpr uint32_t PICO_Freq=380; //PM_SYS_CLK;


extern "C" const uint8_t _binary_optionrom_bin_start[];
extern "C" const uint8_t _binary_optionrom_bin_end[];
extern "C" const uint8_t _binary_optionrom_bin_size[];

[[gnu::section(".core1_static")]]
static uint32_t read_fn(void* obj, uint32_t faddr)
{
	volatile uint8_t * arr = static_cast<volatile uint8_t*>(obj);
	uint8_t ret = arr[faddr];
	return ret;
}

[[gnu::section(".core1_static")]]
static void nop_wrfn(void* obj, uint32_t faddr, uint8_t data) {}

static void copy_optionroms(void) {
  extern char __core1_static_start__[];
  extern char __core1_static_end__[];
  extern char __core1_static_source__[];

  memcpy(__core1_static_start__, __core1_static_source__, (__core1_static_end__-__core1_static_start__));
}

int main(void)
{

	// Overclock!
	vreg_set_voltage(VREG_VOLTAGE_1_25);
	sleep_ms(100);
	set_sys_clock_khz(PICO_Freq*1000, true);

	ISA_Pre_Init();

	Config::Flash_Storage::load();

	ISA_Init();

	tusb_init();

	network_init();

	www_init();
	copy_optionroms();
	add_device({
					.start = 0xC8000,
					.size = (uint32_t)_binary_optionrom_bin_size,
					.type = Device::Type::MEM,
					.rdfn = read_fn,
					.wrfn = nop_wrfn,
					.obj = (void*)_binary_optionrom_bin_start,
	});


	bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_R_BITS | BUSCTRL_BUS_PRIORITY_DMA_W_BITS;
	ISA_Start();

	while(1)
	{
		tud_task(); // tinyusb device task
		network_poll();
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
