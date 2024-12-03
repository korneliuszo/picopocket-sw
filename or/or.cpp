#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <isa_worker.hpp>
#include <ioiface.hpp>
#include "or.hpp"
#include "or_internal.hpp"

static uint32_t read_fn(void* obj, uint32_t faddr)
{
	volatile uint8_t * arr = static_cast<volatile uint8_t*>(obj);
	uint8_t ret = arr[faddr];
	return ret;
}

static void nop_wrfn(void* obj, uint32_t faddr, uint8_t data) {}

extern "C" const uint8_t _binary_optionrom_bin_start[];
extern "C" const uint8_t _binary_optionrom_bin_size[];
extern "C" const uint8_t _binary_optionrom_bin_end[];

volatile uint8_t monitor_data_window[2048];

extern const IoIface::Handler optionrom_handler;

extern "C" OROMHandler __start_or_handlers;
extern "C" OROMHandler __stop_or_handlers;

struct ORHandler : public IoIface::OHandler {
	enum class STATE {
		NO_OP,
		READ_ENTRY,
		READ_IRQ_NO,
		READ_ENTRY_REGS,
		ARBITRATION,
		SEND_CMD,
		SEND_DATA,
		RECV_DATA,
	};
	STATE state;
	Thread_SHM thread_data;
	volatile Thread_SHM::ENTRY thread_entry;
	volatile bool thread_stopping;
	volatile bool thread_state = false;
	OROMHandler *itr_hndlr;
	IoIface::Arbitration arbiter;
	int bytes;
	virtual void wr_byte(uint8_t byte)
	{
		switch (state){
		case STATE::NO_OP:
		default:
			return;
		case STATE::READ_ENTRY:
			thread_data.entry.entry = byte;
			state = STATE::READ_IRQ_NO;
			return;
		case STATE::READ_IRQ_NO:
			thread_data.entry.irq_no = byte;
			itr_hndlr=&__start_or_handlers; //could be later with new if
			bytes = 0;
			state = STATE::READ_ENTRY_REGS;
			return;
		case STATE::READ_ENTRY_REGS:
			thread_data.entry.regs.data[bytes++] = byte; //no endianess check
			if(bytes != sizeof(thread_data.entry.regs))
				return;
			while(itr_hndlr < &__stop_or_handlers)
			{
				if(itr_hndlr->decide(thread_data.entry))
				{
					thread_entry = reinterpret_cast<Thread_SHM::ENTRY>(itr_hndlr->entry);
					itr_hndlr++;
					arbiter.reset();
					state = STATE::ARBITRATION;
					return;
				}
				itr_hndlr++;
			}
			state = STATE::NO_OP;
			return;
		case STATE::ARBITRATION:
		 {
			auto ret=arbiter.wr_byte(byte);
			switch(ret){
			case IoIface::Arbitration::RET::CONTINUE:
				return;
			case IoIface::Arbitration::RET::WON:
				state = STATE::SEND_CMD;
				return;
			default:
			case IoIface::Arbitration::RET::LOST:
				state = STATE::NO_OP;
				return;
			}
			return; //pedantic
		 }
		case STATE::RECV_DATA:
			thread_data.cmd.recv->buff[bytes++] = byte;
			if(bytes == thread_data.cmd.recv->len)
			{
				bytes = 0;
				if(thread_data.cmd.recv->next)
				{
					thread_data.cmd.recv = thread_data.cmd.recv->next;
					return;
				}
				state = STATE::SEND_CMD;
				thread_data.cmd.command = 0;
			}
		}
	}
	virtual IoIface::Handler_return_t rd_byte()
	{
		switch(state)
		{
		default:
			return ISA_FAKE_READ;
		case STATE::ARBITRATION:
			return arbiter.rd_byte();
		case STATE::SEND_CMD:
		 {
			uint8_t cmd;
			if((cmd = thread_data.cmd.command))
			{
				if(cmd == 0x03) //continue boot
				{
					while(itr_hndlr < &__stop_or_handlers)
					{
						if(itr_hndlr->decide(thread_data.entry))
						{
							thread_data.cmd.command = 0x00; //notYET
							thread_entry = reinterpret_cast<Thread_SHM::ENTRY>(itr_hndlr->entry);
							thread_stopping = true;
							itr_hndlr++;
							return 0x00; //override
						}
						itr_hndlr++;
					}
				}
				state =STATE::SEND_DATA;
				bytes = 0;
				if(!thread_data.cmd.send)
				{
					state = STATE::RECV_DATA;
					if(!thread_data.cmd.recv)
					{
						state = STATE::SEND_CMD;
						thread_data.cmd.command = 0;
					}
				}
			}
			return cmd;
		 }
		case STATE::SEND_DATA:
			uint8_t ret = thread_data.cmd.send->buff[bytes++]; //no endianess check
			if(bytes == thread_data.cmd.send->len)
			{
				bytes = 0;
				if(thread_data.cmd.send->next)
				{
					thread_data.cmd.send = thread_data.cmd.send->next;
					return ret;
				}
				state = STATE::RECV_DATA;
				if(!thread_data.cmd.recv)
				{
					state = STATE::SEND_CMD;
					thread_data.cmd.command = 0;
				}
			}
			return ret;
		}
	}
	virtual const IoIface::Handler & ec() { return optionrom_handler;};
	void reset()
	{
		state = STATE::READ_ENTRY;
		thread_stopping = true;
	}
};

static ORHandler handles[4];

static IoIface::OHandler * acquire_object(uint8_t pipe)
{
	handles[pipe].reset();
	return &handles[pipe];
}
static void release_object(IoIface::OHandler * handle)
{
	reinterpret_cast<ORHandler*>(handle)->reset();
}

Handler_type_section
IoIface::Handler optionrom_handler = {
		0x01,
		acquire_object,
		release_object
};

void optionrom_install(Thread * main)
{
	memcpy((void *)monitor_data_window,(const void*)_binary_optionrom_bin_start,sizeof(monitor_data_window));

	monitor_install(main);
	int19_install(main);

	add_device({
					.start = 0xDC000,
					.size = (uint32_t)sizeof(monitor_data_window),
					.type = Device::Type::MEM,
					.rdfn = read_fn,
					.wrfn = nop_wrfn,
					.obj = (void*)monitor_data_window,
	});
}

void optionrom_start_worker(Thread * main)
{
	for(size_t i=0;i<4;i++)
	{
		if(handles[i].thread_stopping && handles[i].thread_state)
		{
			handles[i].thread_state = false;
			handles[i].thread_data.disconnect(); //we aren't even in disconnected task
		}
		handles[i].thread_stopping = false;

		if(handles[i].thread_entry && !handles[i].thread_state)
		{
			handles[i].thread_state = true;
			handles[i].thread_data.run(main,handles[i].thread_entry);
			handles[i].thread_entry = nullptr;
		}
	}
}
