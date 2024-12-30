#include "sbdsp_magic_numbers.hpp"

#define DSP_DMA_FIFO_SIZE       1024

#include "isa_worker.hpp"
#include "audio_dma.hpp"
#include "fifo.hpp"
#include "audio.hpp"
#include <atomic>

static bool rx_avail;
static bool tx_avail;

static uint8_t irq_hndl;

enum class PLAYBACK_ENGINE {
	NONE,
	STATIC,
	DMA,
};

volatile PLAYBACK_ENGINE req_playback;

enum class State {
	RESET_ACK,
	RX_CMD,
	RX_DSP_DIRECT_DAC,
	TX_DSP_DIRECT_ADC,
	RX_DSP_SET_TIME_CONSTANT,
	RX_DSP_DMA_SINGLE_LOW,
	RX_DSP_DMA_SINGLE_HIGH,
	RX_DSP_DMA_BLOCK_SIZE_LOW,
	RX_DSP_DMA_BLOCK_SIZE_HIGH,
	TX_DSP_VERSION_MAJOR,
	TX_DSP_VERSION_MINOR,
};

static State sbdsp_state;

static uint32_t read_fn(void* obj, uint32_t faddr)
{
	//__breakpoint();
	switch(faddr)
	{
	case DSP_READ_STATUS:
		IRQ_Set(irq_hndl,false);
		return (rx_avail<<7) | DSP_UNUSED_STATUS_BITS_PULLED_HIGH;
	case DSP_WRITE_STATUS:
		return (!tx_avail<<7) | DSP_UNUSED_STATUS_BITS_PULLED_HIGH;
		break;
	case DSP_READ:
	{
		switch(sbdsp_state)
		{
		default:
			return ISA_FAKE_READ; // should not happen
		case State::RESET_ACK:
			sbdsp_state = State::RX_CMD;
			rx_avail = false;
			tx_avail = true;
			return 0xAA;
		case State::TX_DSP_DIRECT_ADC:
			sbdsp_state = State::RX_CMD;
			rx_avail = false;
			tx_avail = true;
			return 0x80;
		case State::RX_CMD:
			return ISA_FAKE_READ; // should not happen
		case State::TX_DSP_VERSION_MAJOR:
			sbdsp_state = State::TX_DSP_VERSION_MINOR;
			return DSP_VERSION_MAJOR;
		case State::TX_DSP_VERSION_MINOR:
			sbdsp_state = State::RX_CMD;
			rx_avail = false;
			tx_avail = true;
			return DSP_VERSION_MINOR;
		}
	}
	default:
		return ISA_FAKE_READ;
	}
}

static volatile uint32_t static_out;

static volatile uint32_t decimationx1000;

static volatile bool single_mode;
static volatile uint32_t single_len;

static volatile uint32_t block_size;
static volatile uint32_t block_pos;

static uint8_t single_len_lo;
static uint8_t block_size_lo;


static void write_fn(void* obj, uint32_t faddr, uint8_t data)
{
	//__breakpoint();
	switch(faddr)
	{
	case DSP_RESET:
	{
		if(data&1)
		{
			sbdsp_state = State::RESET_ACK;
			rx_avail = true;
			tx_avail = true;
			//reset
		}
	}	break;
	case DSP_WRITE:
	{
		switch(sbdsp_state)
		{
		default:
			__breakpoint();
			return; // should not happen
		case State::RESET_ACK:
			rx_avail = false;
			sbdsp_state = State::RX_CMD;
		case State::RX_CMD:
			switch(data)
			{
			default: //not known!!
				__breakpoint();
				return;
			case DSP_DIRECT_DAC:
				sbdsp_state = State::RX_DSP_DIRECT_DAC;
				req_playback = PLAYBACK_ENGINE::STATIC;
				return;
			case DSP_DIRECT_ADC:
				sbdsp_state = State::TX_DSP_DIRECT_ADC;
				rx_avail = true;
				tx_avail = false;
			case DSP_SET_TIME_CONSTANT:
				sbdsp_state = State::RX_DSP_SET_TIME_CONSTANT;
				return;
			case DSP_DMA_SINGLE:
				sbdsp_state = State::RX_DSP_DMA_SINGLE_LOW;
				return;
			case DSP_DMA_AUTO:
			case DSP_DMA_HS_AUTO:
				single_mode = false;
				req_playback = PLAYBACK_ENGINE::DMA;
				sbdsp_state = State::RX_CMD;
				return;
			case DSP_DMA_HS_SINGLE:
				single_len = block_size;
				single_mode = true;
				req_playback = PLAYBACK_ENGINE::DMA;
				sbdsp_state = State::RX_CMD;
				return;
			case DSP_DMA_BLOCK_SIZE:
				sbdsp_state = State::RX_DSP_DMA_BLOCK_SIZE_LOW;
				return;
			case DSP_ENABLE_SPEAKER:
			case DSP_DISABLE_SPEAKER:
				return;
			case DSP_DMA_PAUSE:
				req_playback = PLAYBACK_ENGINE::NONE;
				return;
			case DSP_DMA_RESUME:
				req_playback = PLAYBACK_ENGINE::DMA;
				return;
			//case DSP_IDENT:
			case DSP_VERSION:
				tx_avail = false;
				rx_avail = true;
				sbdsp_state = State::TX_DSP_VERSION_MAJOR;
				return;
			} return;
			case State::RX_DSP_DIRECT_DAC:
			{
				int16_t sample =
						((static_cast<int16_t>(data)<<8)+(-0x8000));
				uint16_t container = static_cast<uint16_t>(sample);
				static_out = container | container << 16;
				sbdsp_state = State::RX_CMD;
				return;
			} break;
			case State::RX_DSP_SET_TIME_CONSTANT:
			{
				decimationx1000 = 384*(256-data);
				sbdsp_state = State::RX_CMD;
				return;
			} break;
			case State::RX_DSP_DMA_SINGLE_LOW:
			{
				single_len_lo = data;
				sbdsp_state = State::RX_DSP_DMA_SINGLE_HIGH;
				return;
			} break;
			case State::RX_DSP_DMA_SINGLE_HIGH:
			{
				single_len = (single_len_lo | data<<8)+1;
				single_mode = true;
				req_playback = PLAYBACK_ENGINE::DMA;
				sbdsp_state = State::RX_CMD;
				return;
			} break;
			case State::RX_DSP_DMA_BLOCK_SIZE_LOW:
			{
				block_size_lo = data;
				sbdsp_state = State::RX_DSP_DMA_BLOCK_SIZE_HIGH;
				return;
			} break;
			case State::RX_DSP_DMA_BLOCK_SIZE_HIGH:
			{
				block_size = (block_size_lo | data<<8)+1;
				sbdsp_state = State::RX_CMD;
				return;
			} break;
		}
	}	break;
	default:
		break;
	}
}



void sbdsp_install(Thread * main)
{
	AudioDMA::AudioDMA::init();

	irq_hndl = IRQ_Create_Handle(5);

	add_device({
					.start = 0x220,
					.size = (uint32_t)0x10,
					.type = Device::Type::IO,
					.rdfn = read_fn,
					.wrfn = write_fn,
					.obj = nullptr,
	});
}


static std::atomic<bool> dma_playback_stop;

static const int16_t* __not_in_flash_func(dma_get_buff)(size_t req_buff)
{
	if(dma_playback_stop)
		return nullptr;
	static int16_t buff[2][AudioDMA::AudioDMA::DMA_BYTE_LEN/2];
	static size_t curr_buff;
	if(++curr_buff>=2)
		curr_buff = 0;

	for(int i=0;i<AudioDMA::AudioDMA::DMA_BYTE_LEN/2;i+=2)
	{
		static int16_t sample;
		static uint32_t frac=0;
		static uint32_t block;
		if((frac+=1000)>=decimationx1000)
		{
			frac-=decimationx1000;
			if(++block >= block_size)
			{
				block = 0;
				IRQ_Set(irq_hndl,true);
			}
			if(single_mode)
			{
				if(single_len-- == 0)
				{
					return nullptr;
				}
			}
			if(DMA_RX_is_ready())
			{
				sample =
						((static_cast<int16_t>(DMA_RX_get())<<8)+(-0x8000));
			}
			else
			{
				//underflow
			}
		}
		buff[curr_buff][i] = buff[curr_buff][i+1] = sample;
	}

	return buff[curr_buff];
}

using dma_playback = AudioDMA::AudioDMA::GetbuffISR_playback<&dma_get_buff>;

void sbdsp_poll(Thread * main)
{
	static PLAYBACK_ENGINE current_playback;
	if(current_playback != req_playback)
	{
		switch(current_playback)
		{
		case PLAYBACK_ENGINE::NONE:
		default:
			break;
		case PLAYBACK_ENGINE::STATIC:
			dma_channel_abort(AudioDMA::AudioDMA::ping_dma_chan);
			dma_channel_abort(AudioDMA::AudioDMA::pong_dma_chan);
			dma_channel_abort(AudioDMA::AudioDMA::ping_dma_chan);
			break;
		case PLAYBACK_ENGINE::DMA:
			dma_playback_stop = true;
			if(!dma_playback::is_complete())
				return;
			break;
		}
		switch((current_playback = req_playback))
		{
		case PLAYBACK_ENGINE::NONE:
		default:
			break;
		case PLAYBACK_ENGINE::STATIC:
			AudioDMA::AudioDMA::update_pio_frequency(384000);
			AudioDMA::AudioDMA::setup_dma_const(
					AudioDMA::AudioDMA::ping_dma_chan,
					AudioDMA::AudioDMA::pong_dma_chan,
					const_cast<uint32_t*>(&static_out)
					);
			dma_channel_start(AudioDMA::AudioDMA::ping_dma_chan);
			break;
		case PLAYBACK_ENGINE::DMA:
			dma_playback_stop = false;
			DMA_RX_Setup();
			dma_playback::init_playback(384000);
			break;
		}
	}
}


