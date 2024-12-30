#include "sbdsp_magic_numbers.hpp"

#define DSP_DMA_FIFO_SIZE       1024

#include "isa_worker.hpp"
#include "audio_dma.hpp"
#include "fifo.hpp"
#include "audio.hpp"
#include <atomic>

static bool rx_avail;
static bool tx_avail;

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
};

static State sbdsp_state;

static uint32_t read_fn(void* obj, uint32_t faddr)
{
	//__breakpoint();
	switch(faddr)
	{
	case DSP_READ_STATUS:
		//lower irq
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
		case State::RX_CMD:
			return ISA_FAKE_READ; // should not happen
		}
	}
	default:
		return ISA_FAKE_READ;
	}
}

volatile uint32_t static_out;

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
			return; // should not happen
		case State::RESET_ACK:
			rx_avail = false;
			sbdsp_state = State::RX_CMD;
		case State::RX_CMD:
			switch(data)
			{
			default: //not known!!
				return;
			case DSP_DIRECT_DAC:
				sbdsp_state = State::RX_DSP_DIRECT_DAC;
				req_playback = PLAYBACK_ENGINE::STATIC;
				return;
			}
			return;
		case State::RX_DSP_DIRECT_DAC:
		{
			int16_t sample =
					((static_cast<int16_t>(data)<<8)+(-0x8000));
			uint16_t container = static_cast<uint16_t>(sample);
			static_out = container | container << 16;
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
static const int16_t* dma_get_buff(size_t req_buff)
{
	if(dma_playback_stop)
		return nullptr;
	static int16_t buff[2][AudioDMA::AudioDMA::DMA_BYTE_LEN/2];
	static size_t curr_buff;
	if(++curr_buff>=2)
		curr_buff = 0;

	//int16_t static_out_cached = static_out;
	//for(size_t i=0;i<AudioDMA::AudioDMA::DMA_BYTE_LEN/2;i++)
	//	buff[curr_buff][i] = static_out_cached;

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
			dma_playback::init_playback(384000);
			break;
		}
	}
}


