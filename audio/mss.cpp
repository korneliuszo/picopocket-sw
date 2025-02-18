#include "isa_worker.hpp"
#include "audio.hpp"
#include "audio_dma.hpp"

typedef struct ad1848_t {
    union {
        struct {
            uint8_t linp;  //  0
            uint8_t rinp;  //  1
            uint8_t laux1; //  2
            uint8_t raux1; //  3
            uint8_t laux2; //  4
            uint8_t raux2; //  5
            uint8_t lout;  //  6
            uint8_t rout;  //  7
            union {
            	uint8_t data;
            	struct {
            		uint8_t sample:4;
            		uint8_t sm:1;
            		uint8_t lc:1;
            		uint8_t fmt:1;
            		uint8_t res:1;
            	};
            } dform;       //  8
            union {
            	uint8_t data;
            	struct {
            		uint8_t pen:1;
            		uint8_t cen:1;
            		uint8_t sdc:1;
            		uint8_t acal:1;
            		uint8_t res:2;
            		uint8_t ppio:1;
            		uint8_t cpio:1;
            	};
            } iface;       //  9
            union {
            	uint8_t data;
            	struct {
            		uint8_t res:1;
            		uint8_t ien:1;
            		uint8_t res2:6;
            	};
            } pinc;        // 10
            union {
            	uint8_t data;
            	struct {
            		uint8_t orl:2;
            		uint8_t orr:2;
            		uint8_t drs:1;
            		uint8_t aci:1;
            		uint8_t pur:1;
            		uint8_t cor:1;
            	};
            } init;        // 11
            uint8_t misc;  // 12
            uint8_t mix;   // 13
            uint8_t ubase; // 14
            uint8_t lbase; // 15
        };
        uint8_t idx[16];
    } regs;

    uint8_t idx_addr;   // Register index address
    bool irq_pending;

    bool mce;           // Mode change enable - mutes output during mode changes
    bool trd;           // Transfer request disable
    bool aci;

    bool playing;

    enum class PIO_byte {
    	L_L,
    	L_U,
    	R_L,
    	R_U,
    	CPL,
    };

    uint32_t ppio_sample;
    PIO_byte ppio_sample_state;

    uint16_t current_count;
    uint16_t current_count_left;
} ad1848_t;

static ad1848_t ad1848 = {
    .regs = {
        .laux1 = 0b10000000,
        .raux1 = 0b10000000,
        .laux2 = 0b10000000,
        .raux2 = 0b10000000,
        .lout  = 0b10000000,
        .rout  = 0b10000000,
        .iface = 0b00001000,
        .misc  = 0b00001010
    },
}; // all other values to 0

static uint8_t irq_hndl;

static constexpr uint16_t sample_rates[] = {
    8000,
    5513,
    16000,
    11025,
    27429,
    18900,
    32000,
    22050,
    54857, // Unsupported
    37800,
    64000, // Unsupported
    44100,
    48000,
    33075,
    9600,
    6615
};



static uint32_t read_fn(void* obj, uint32_t faddr)
{
	switch(faddr&0x03)
	{
	default:
		return ISA_FAKE_READ;
	case 0:
		return ad1848.idx_addr | (ad1848.mce ? 0x40 : 0) | (ad1848.trd ? 0x20 : 0);
	case 1:
        return ad1848.regs.idx[ad1848.idx_addr];
	case 2:
	{
	    bool plr = ad1848.ppio_sample_state == ad1848.PIO_byte::L_L
	    		|| ad1848.ppio_sample_state == ad1848.PIO_byte::L_U;
	    bool pul = ad1848.ppio_sample_state == ad1848.PIO_byte::R_U
	    		|| ad1848.ppio_sample_state == ad1848.PIO_byte::L_U;
	    bool prdy = ad1848.ppio_sample_state != ad1848.PIO_byte::CPL;
		return  (ad1848.irq_pending<<0) |
				(prdy<<1) | // PRDY
				(plr<<2) | // PL/R
				(pul<<3) | // PU/L
				((ad1848.regs.init.cor | ad1848.regs.init.pur)<<4) | // SOUR
				(0<<5) | // CRDY
				(1<<6) | // CL/R
				(1<<7); // CU/L
	}
	case 3:
		return 0x00; // PIO read
	}
}

static void write_fn(void* obj, uint32_t faddr, uint8_t data)
{
	switch(faddr&0x03)
	{
	default:
		return;
	case 0:
	{
		ad1848.idx_addr = data & 0xf;
		bool new_mce = data & 0x40;
        if (!new_mce && ad1848.mce) { // exiting mce
            //ad1848.aci = true;
        }
        ad1848.mce = new_mce;
        ad1848.trd = data & 0x20;
        return;
	}
	case 1:
        if (ad1848.idx_addr == 12)
            return;
        if (ad1848.idx_addr != 11) {
            ad1848.regs.idx[ad1848.idx_addr] = data;
        }
        if (ad1848.idx_addr == 10)
        {
        	if(ad1848.regs.pinc.ien)
        		IRQ_Set(irq_hndl,ad1848.irq_pending);
        	else
        		IRQ_Set(irq_hndl,false);
        }
        if (ad1848.idx_addr == 14)
        {
            ad1848.current_count = ((ad1848.regs.ubase << 8) | ad1848.regs.lbase) + 1;
            ad1848.current_count_left = ad1848.current_count;
        }
        return;
	case 2:
        ad1848.irq_pending = false;
		IRQ_Set(irq_hndl,false);
		return;
	case 3:
		switch(ad1848.ppio_sample_state)
		{
		default:
		case ad1848.PIO_byte::CPL:
			return;
		case ad1848.PIO_byte::L_L:
			ad1848.ppio_sample = data;
			if(!ad1848.regs.dform.sm)
				ad1848.ppio_sample |= data <<16;
			ad1848.ppio_sample_state = ad1848.PIO_byte::L_U;
			return;
		case ad1848.PIO_byte::L_U:
			if(ad1848.regs.dform.fmt)
			{
				ad1848.ppio_sample |= data <<8;
				if(!ad1848.regs.dform.sm)
				{
					ad1848.ppio_sample |= data <<24;
					ad1848.ppio_sample_state = ad1848.PIO_byte::CPL;
				}
				else
				{
					ad1848.ppio_sample_state = ad1848.PIO_byte::R_L;
				}
			}
			else
			{
				ad1848.ppio_sample = ((uint16_t)(data-0x80))<<8;
				if(!ad1848.regs.dform.sm)
				{
					ad1848.ppio_sample |= ((uint16_t)(data-0x80))<<24;
					ad1848.ppio_sample_state = ad1848.PIO_byte::CPL;
				}
				else
				{
					ad1848.ppio_sample_state = ad1848.PIO_byte::R_U;
				}
			}
			return;
		case ad1848.PIO_byte::R_L:
			ad1848.ppio_sample |= data<<16;
			ad1848.ppio_sample_state = ad1848.PIO_byte::R_U;
			return;
		case ad1848.PIO_byte::R_U:
			if(ad1848.regs.dform.fmt)
			{
				ad1848.ppio_sample |= data <<24;
				ad1848.ppio_sample_state = ad1848.PIO_byte::CPL;
			}
			else
			{
				ad1848.ppio_sample |= ((uint16_t)(data-0x80))<<24;
				ad1848.ppio_sample_state = ad1848.PIO_byte::CPL;
			}
			return;
		}
	}
}

void mss_install(Thread * main)
{
	irq_hndl = IRQ_Create_Handle(11);

	add_device({
					.start = 0x530,
					.size = (uint32_t)8,
					.type = Device::Type::IO,
					.rdfn = read_fn,
					.wrfn = write_fn,
					.obj = nullptr,
	});
}

static void __not_in_flash_func(isr_pio)()
{
	volatile PIO pio = pio1;

	if (ad1848.trd && !ad1848.current_count_left) {
		pio_sm_put(pio,AudioDMA::AudioDMA::pio_sm, 0);
	}

	if(ad1848.ppio_sample_state == ad1848.PIO_byte::CPL)
	{
		ad1848.regs.init.pur = 0;
		pio_sm_put(pio,AudioDMA::AudioDMA::pio_sm, ad1848.ppio_sample);
		ad1848.ppio_sample_state = ad1848.regs.dform.fmt ? ad1848.PIO_byte::L_U : ad1848.PIO_byte::L_L;
	}
	else
	{
		ad1848.regs.init.pur = 1;
		pio_sm_put(pio,AudioDMA::AudioDMA::pio_sm, 0);
	}
	ad1848.current_count_left--;
    if(!ad1848.current_count_left)
    {
        ad1848.irq_pending = true;
    	if(ad1848.regs.pinc.ien)
    		IRQ_Set(irq_hndl,true);
    	ad1848.current_count_left = ad1848.current_count;
    }

}

static void __not_in_flash_func(isr)()
{
	volatile PIO pio = pio1;

	if (ad1848.trd && !ad1848.current_count_left) {
		pio_sm_put(pio,AudioDMA::AudioDMA::pio_sm, 0);
	}

	uint needed_data;
	if(ad1848.regs.dform.fmt)
	{
		if(ad1848.regs.dform.sm)
			needed_data=4;
		else
			needed_data=2;
	}
	else
	{
		if(ad1848.regs.dform.sm)
			needed_data=2;
		else
			needed_data=1;
	}
	uint32_t sample;
	if(DMA_RX_ready_data()<needed_data)
	{ //underrun
		ad1848.regs.init.pur = 1;
		sample = 0;
	}
	else
	{
		ad1848.regs.init.pur = 0;
		if(ad1848.regs.dform.fmt)
		{
			uint8_t lsb=DMA_RX_get();
			uint16_t msb=DMA_RX_get();
			sample = msb<<8 | lsb;
		}
		else
		{
			sample = ((int16_t)DMA_RX_get())-0x80;
		}
		if(ad1848.regs.dform.sm)
		{
			if(ad1848.regs.dform.fmt)
			{
				uint8_t lsb=DMA_RX_get();
				uint16_t msb=DMA_RX_get();
				sample |= (msb<<8 | lsb)<<16;
			}
			else
			{
				sample |= (((int16_t)DMA_RX_get())-0x80)<<16;
			}
		}
		else
			sample = sample | sample <<16;
	}

	pio_sm_put(pio,AudioDMA::AudioDMA::pio_sm, sample);
    ad1848.current_count_left--;
    if(!ad1848.current_count_left)
    {
        ad1848.irq_pending = true;
    	if(ad1848.regs.pinc.ien)
    		IRQ_Set(irq_hndl,true);
    	if (!ad1848.trd)
    		ad1848.current_count_left = ad1848.current_count;
    }
}

void mss_poll(Thread * main)
{

	if(!ad1848.playing && ad1848.regs.iface.pen)
	{
		ad1848.playing = true;
		ad1848.ppio_sample_state = ad1848.regs.dform.fmt ? ad1848.PIO_byte::L_U : ad1848.PIO_byte::L_L;
		DMA_RX_Setup();
		AudioDMA::AudioDMA::init();
		AudioDMA::AudioDMA::update_pio_frequency(sample_rates[ad1848.regs.dform.sample]);
	    if(ad1848.regs.iface.ppio)
	    	irq_add_shared_handler(PIO1_IRQ_1, isr_pio, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY); // Add a shared IRQ handler
	    else
	    	irq_add_shared_handler(PIO1_IRQ_1, isr, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY); // Add a shared IRQ handler
	    irq_set_enabled(PIO1_IRQ_1, true); // Enable the IRQ
	    pio_set_irqn_source_enabled(pio1, 1,
	    		(pio_interrupt_source)((uint)pis_sm0_tx_fifo_not_full+AudioDMA::AudioDMA::pio_sm), true); // Set pio to tell us when the FIFO is NOT full

	}
	if(ad1848.playing && !ad1848.regs.iface.pen)
	{
		ad1848.playing = false;
	    irq_set_enabled(PIO1_IRQ_1, false);
		ad1848.ppio_sample_state = ad1848.PIO_byte::CPL;
	    if(ad1848.regs.iface.ppio)
		    irq_remove_handler(PIO1_IRQ_1,isr_pio);
	    else
	    	irq_remove_handler(PIO1_IRQ_1,isr);
		AudioDMA::AudioDMA::deinit();
	}


}
