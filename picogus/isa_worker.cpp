/*
 * SPDX-FileCopyrightText: Korneliusz Osmenda <korneliuszo@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "hardware/structs/sio.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "pico/multicore.h"
#include <array>
#include <cstring>
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "hardware/interp.h"
#include "hardware/sync.h"

#include "isa_dma_gus.pio.h"

int dma_helper_rd_offset = -1;
int dma_helper_wr_offset = -1;

#include <isa_worker.hpp>

constexpr uint TC_PIN = 20;
constexpr uint IA0_PIN = 21;

[[gnu::section(".core1")]] volatile uint8_t io_map[2*1024/8]; //8 IO regs resolution

volatile bool dma_single_transfer;

critical_section_t IRQ_setter;

void ISA_Pre_Init()
{
	// ************ Pico I/O Pin Initialisation ****************
	// * Put the I/O in a correct state to avoid PC Crash      *

	gpio_init(PIN_DRQ);
	gpio_put(PIN_DRQ, 0);
	gpio_set_dir(PIN_DRQ,true);

	//IRQ
	for(uint pin=IA0_PIN;pin<IA0_PIN+1;pin++)
	{
		gpio_init(pin);
		gpio_put(pin, 0);
		gpio_set_dir(pin,true);
	}

	memset((void*)io_map,0,sizeof(io_map));
	critical_section_init(&IRQ_setter);
}

volatile bool TC_triggered_val;

static void TC_trigger_IRQ(void)
{
    if (gpio_get_irq_event_mask(TC_PIN)) {
       gpio_acknowledge_irq(TC_PIN, GPIO_IRQ_EDGE_RISE);
       TC_triggered_val = true;
    }
}

bool TC_Triggered()
{
	if(TC_triggered_val)
	{
		TC_triggered_val = false;
		return true;
	}
	return false;
}

void SetupSingleTransferTXDMA(uint dma_chan, const volatile uint8_t * buff, size_t len)
{
	initialize_dma_rd_pio(pio0);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio0, 1, true));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);

    dma_channel_configure(dma_chan, &c,
    	&pio0->txf[2],      // Destination pointer
		buff,               // Source pointer
        len,                // Number of transfers
        true                // Start immediately
    );

    dma_single_transfer = true;
	TC_triggered_val = false;
}

void SetupSingleTransferRXDMA(uint dma_chan, volatile uint8_t * buff, size_t len)
{
	initialize_dma_wr_pio(pio0);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio0, 2, false));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);

    dma_channel_configure(dma_chan, &c,
    	buff,               // Destination pointer
		&pio0->rxf[2],      // Source pointer
        len,                // Number of transfers
        true                // Start immediately
    );

    dma_single_transfer = true;
	TC_triggered_val = false;
}

void DMA_RX_Setup()
{
	initialize_dma_wr_pio(pio0);
}

void DMA_TX_Setup()
{
	initialize_dma_rd_pio(pio0);
}

bool __not_in_flash_func(DMA_RX_is_ready)()
{
	return ! pio_sm_is_rx_fifo_empty(pio0,2);
}

uint __not_in_flash_func(DMA_RX_ready_data)()
{
	return pio_sm_get_rx_fifo_level(pio0,2);
}

uint8_t __not_in_flash_func(DMA_RX_get)()
{
	return pio_sm_get(pio0,2);
}

void __not_in_flash_func(DMA_TX_put)(uint8_t val)
{
	pio_sm_put(pio0,2,val);
}

uint __not_in_flash_func(DMA_TX_ready_data)()
{
	return 8-pio_sm_get_tx_fifo_level(pio0,2);
}

bool DMA_Complete(uint dma_chan)
{
	return !dma_channel_is_busy(dma_chan);
}

void ISA_Init()
{
	//TC line
	gpio_init(TC_PIN);
	gpio_set_irq_enabled(TC_PIN, GPIO_IRQ_EDGE_RISE, true);
	gpio_add_raw_irq_handler_masked((1<<TC_PIN), TC_trigger_IRQ);
	irq_set_enabled(IO_IRQ_BANK0, true);

	initialize_isa_pio(pio0);
}

struct Device_int
{
	uint32_t start;
	uint32_t (*rdfn)(void*, uint32_t);
	void (*wrfn)(void*, uint32_t,uint8_t);
	void * obj;
};


std::array<Device_int,4> devices_io = {};

std::atomic<bool> wait_for_flash;

size_t used_devices_io = 0;

bool add_device(const Device & device)
{
	switch(device.type)
	{
	case Device::Type::MEM:
		return false;
		break;
	case Device::Type::IO:
		if(used_devices_io == devices_io.size())
			return false;
		{
			uint16_t start = device.start&0x3ff; //up to A9 decoded
			devices_io[used_devices_io] =
			{
					.start = start,
					.rdfn = device.rdfn,
					.wrfn = device.wrfn,
					.obj = device.obj,
			};
			for(uint32_t page=start;page<start+device.size;page+=8)
			{
				uint32_t idx = page>>3;
				io_map[idx] = 1+used_devices_io;
			}
		}
		used_devices_io++;

		break;
	default:
		return false;
	}
	return true;
}


static void
__scratch_x("core1_code")
[[gnu::noreturn]] main_core1(void)
{
	uint32_t TMP = 0x00000;
	uint32_t TMP2 = 0x01;
	for (;;)
	{
		uint32_t ISA_TRANS, ISA_IDX;

		PIO isa_pio = pio0;

		uint32_t IORDY = (3u << (PIO_FSTAT_RXEMPTY_LSB + 0));
		uint32_t IORDYRD = (2u << (PIO_FSTAT_RXEMPTY_LSB + 0));


		// ** Wait until a Control signal is detected **
		asm volatile goto (
				"cmp %[TMP2], #0 \n\t"
				"beq prepare_read \n\t"
				"ldr %[TMP2], =%[IOT] \n\t"
				"str     %[TMP],[%[PIOB],%[PIOTW]] \n\t" // pio->txf[0] = Yreg - turn on RDY
				"b 1f\n\t"
				"prepare_read:\n\t"
				"ldr %[TMP2], =%[IOT] \n\t"
				"str     %[TMP],[%[PIOB],%[PIOTR]] \n\t" // pio->txf[0] = Yreg - turn on RDY
				"not_us_retry:\n\t"
				"1:\n\t"
				"ldr %[TMP], [%[PIOB],%[PIOF]]\n\t" // read fstat
				"mvn %[TMP], %[TMP]\n\t"
				"tst %[TMP], %[IORDY]\n\t"			// test flag
				"beq 1b\n\t"						// loop if empty
				"tst %[TMP], %[IORDYRD]\n\t"
				"bne rd\n\t" 							// read

				"wr:\n\t"
				"ldr  %[ISA_TRANS],[%[PIOB],%[PIORW]] \n\t" // read ISA_TRANSACTION from PIO
				"lsr %[TMP], %[ISA_TRANS],#3\n\t"
				"ldrb %[ISA_IDX],[%[TMP2],%[TMP]]\n\t"	// tmp = io_map[tmp]
				"strb %[ISA_IDX],[%[PIOB],%[PIOTW]] \n\t" // pio->txf[0] = pin status;
				"cmp %[ISA_IDX], #0\n\t"
				"beq not_us_retry \n\t"
				"b %l[wraction]\n\t"							  // jump to memaction

				"rd:\n\t"
				"ldr  %[ISA_TRANS],[%[PIOB],%[PIORR]] \n\t" // read ISA_TRANSACTION from PIO
				"lsr %[TMP], %[ISA_TRANS],#3\n\t"
				"ldrb %[ISA_IDX],[%[TMP2],%[TMP]]\n\t"	// tmp = io_map[tmp]
				"strb %[ISA_IDX],[%[PIOB],%[PIOTR]] \n\t" // pio->txf[0] = pin status;
				"cmp %[ISA_IDX], #0\n\t"
				"beq not_us_retry \n\t"
				"b %l[rdaction]\n\t"							  // jump to memaction

				".ltorg\n\t"
				:
				 [ISA_TRANS]"=l" (ISA_TRANS),
				 [ISA_IDX]"=l" (ISA_IDX),
				 [TMP]"+l" (TMP),
				 [TMP2]"+l" (TMP2),
				 [IORDY]"+l" (IORDY),
				 [IORDYRD]"+l" (IORDYRD),
				 [PIOB]"+l"(isa_pio)
				:[PIOTW]"i" (PIO_TXF0_OFFSET),
				 [PIOTR]"i" (PIO_TXF1_OFFSET),
				 [PIORW]"i" (PIO_RXF0_OFFSET),
				 [PIORR]"i" (PIO_RXF1_OFFSET),
				 [IOT]"i" (io_map),
				 [PIOF]"i"(PIO_FSTAT_OFFSET)
				:"cc"
				:wraction,
				 rdaction
				  );            // Tell the compiler that flags are changed

		continue;

		wraction:
		{
			PIO isa_pio = pio0;
			uint32_t FADDR = ISA_TRANS;
			auto && dev = devices_io[ISA_IDX-1];
			uint8_t data = gpio_get_all()>>PIN_AD0;
			while(wait_for_flash);
			dev.wrfn(dev.obj,FADDR - (dev.start),data);
			TMP2=0x01;
		}
		continue;

		rdaction:
		{
			PIO isa_pio = pio0;
			uint32_t FADDR = ISA_TRANS;
			auto && dev = devices_io[ISA_IDX-1];
			uint32_t datard;
			while(wait_for_flash);
			datard = dev.rdfn(dev.obj,FADDR - (dev.start));
			if(datard&ISA_FAKE_READ)
			{
				TMP = 0x000000;
			}
			else
			{
				TMP= 0x00ff00 | (datard&0xff);
			}
			TMP2 = 0x00;
		}
		continue;
	}
}

void ISA_Start()
{
	multicore_launch_core1(main_core1);
}

struct IRQh {
	bool lit;
};

static constexpr size_t IRQh_len = 10;

static volatile IRQh irqhandlers[IRQh_len];
static volatile size_t IRQh_used;

void IRQ_Handle_Change_IRQ(uint8_t irqh,uint8_t irq)
{
}


uint8_t IRQ_Create_Handle(uint8_t irq)
{
	if(IRQh_used >= IRQh_len)
		return 0xff;
	IRQ_Handle_Change_IRQ(IRQh_used,irq);
	return IRQh_used++;
}

void IRQ_Set(uint8_t irqh, bool val)
{
	if(irqh >= IRQh_used)
		return;
	critical_section_enter_blocking(&IRQ_setter);
	irqhandlers[irqh].lit = val;
	uint8_t orirq = 0;
	for(size_t i=0;i<IRQh_used;i++) // first handler prioritized
	{
		if(irqhandlers[i].lit)
		{
			orirq = 1;
			break;
		}
	}
	gpio_put_masked(1<<IA0_PIN,0<<IA0_PIN);
	if(orirq)
	{
		for(int i=0;i<250;i++)
			asm volatile("nop");
		gpio_put_masked(1<<IA0_PIN,orirq<<IA0_PIN);
	}
	critical_section_exit(&IRQ_setter);
}

