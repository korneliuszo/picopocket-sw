#include <string.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/vreg.h"
#include "hardware/regs/vreg_and_chip_reset.h"
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "network_l2.hpp"
#include "www.hpp"
#include "config.hpp"

constexpr uint32_t PICO_Freq=380; //PM_SYS_CLK;


int main(void)
{

	// Overclock!
	vreg_set_voltage(VREG_VOLTAGE_1_25);
	sleep_ms(100);
	set_sys_clock_khz(PICO_Freq*1000, true);

	Config::Flash_Storage::load();

	tusb_init();

	network_init();

	www_init();

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
