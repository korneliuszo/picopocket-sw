#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "isa_worker.hpp"
#include <ioiface.hpp>
#include "or.hpp"
#include "or_internal.hpp"
#include "config.hpp"

extern "C" const volatile uint8_t _binary_optionrom_bin_size[];
extern "C" const volatile uint8_t _binary_optionrom_bin_end[];
extern "C" const volatile uint8_t _binary_optionrom_bin_start[];

[[gnu::section(".core1_static")]]
static uint32_t read_fn(void* obj, uint32_t faddr)
{
	if (faddr>(_binary_optionrom_bin_end-_binary_optionrom_bin_start))
		return 0xcc; //hope there is at least halt - should not happen
	uint8_t ret = _binary_optionrom_bin_start[faddr];
	return ret;
}

[[gnu::section(".core1_static")]]
static void nop_wrfn(void* obj, uint32_t faddr, uint8_t data) {}


const OROMHandler * const handlers[] =
{
		&config_handler,
		&ramdisk_handler,
#ifndef PICOPOCKET_SIM
		&disk_mapper_handler,
#endif
		&monitor_handler,
		&int19_handler,
};

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
	ENTRY_STATE entry_state;
	volatile Thread_SHM::ENTRY thread_entry;
	Thread_SHM::ENTRY thread_entry_cache;
	volatile bool thread_stopping;
	//volatile bool thread_state = false;
	const OROMHandler * const *itr_hndlr;
	IoIface::Arbitration arbiter;
	int bytes;
	virtual void wr_byte(uint8_t byte)
	{
		switch (state){
		case STATE::NO_OP:
		default:
			return;
		case STATE::READ_ENTRY:
			entry_state.entry = byte;
			state = STATE::READ_IRQ_NO;
			return;
		case STATE::READ_IRQ_NO:
			entry_state.irq_no = byte;
			itr_hndlr=handlers; //could be later with new if
			bytes = 0;
			state = STATE::READ_ENTRY_REGS;
			return;
		case STATE::READ_ENTRY_REGS:
			entry_state.regs.data[bytes++] = byte; //no endianess check
			if(bytes != sizeof(entry_state.regs.regs))
				return;
			while(itr_hndlr < &handlers[sizeof(handlers)/sizeof(handlers[0])])
			{
				if((*itr_hndlr)->decide(entry_state))
				{
					*const_cast<ENTRY_STATE*>(&thread_data.params) = entry_state;
					thread_entry_cache = reinterpret_cast<Thread_SHM::ENTRY>((*itr_hndlr)->entry);
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
				thread_data.cmd.command = 0x00; //notYET
				thread_entry = thread_entry_cache;
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
					thread_stopping = true;
					thread_data.cmd.command = 0;
					while(itr_hndlr < &handlers[sizeof(handlers)/sizeof(handlers[0])])
					{
						if((*itr_hndlr)->decide(thread_data.params))
						{
							thread_entry = reinterpret_cast<Thread_SHM::ENTRY>((*itr_hndlr)->entry);
							itr_hndlr++;
							return 0x00; //override
						}
						itr_hndlr++;
					}
					state = STATE::NO_OP;
					return cmd;
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
	virtual const IoIface::Handler & ec() { return IoIface::optionrom_handler;};
	void reset()
	{
		state = STATE::READ_ENTRY;
		thread_stopping = true;
	}
};

static ORHandler handles[1];

static IoIface::OHandler * acquire_object(uint8_t pipe)
{
	if(pipe > 1)
		return nullptr;
	handles[pipe].reset();
	return &handles[pipe];
}
static void release_object(IoIface::OHandler * handle)
{
	reinterpret_cast<ORHandler*>(handle)->reset();
}

const IoIface::Handler IoIface::optionrom_handler = {
		0x01,
		acquire_object,
		release_object
};

void optionrom_install(Thread * main)
{

	monitor_install(main);
	int19_install(main);
#ifndef PICOPOCKET_SIM
	disk_mapper_install(main);
#endif
	ramdisk_install(main);
	config_install(main);
	if(Config::BIOS_SEGMENT::val.ival)
		add_device({
					.start = (uint32_t)Config::BIOS_SEGMENT::val.ival<<4,
					.size = (uint32_t)(_binary_optionrom_bin_end-_binary_optionrom_bin_start),
					.type = Device::Type::MEM,
					.rdfn = read_fn,
					.wrfn = nop_wrfn,
					.obj = nullptr,
	});
}

void optionrom_start_worker(Thread * main)
{
	monitor_poll();
	for(size_t i=0;i<1;i++)
	{
		if(handles[i].thread_stopping)
		{
			handles[i].thread_data.disconnect(); //we aren't even in disconnected task
			handles[i].thread_stopping = false;
		}
		if(handles[i].thread_entry && !handles[i].thread_stopping)
		{
			auto tmp = handles[i].thread_entry;
			handles[i].thread_entry = nullptr;
			handles[i].thread_data.run(main,tmp);
		}
	}
}
