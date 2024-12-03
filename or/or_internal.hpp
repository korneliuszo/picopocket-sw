#pragma once
#include <stdint.h>
#include "jmpcoro_configured.hpp"

union IRQ_REGS{
	struct [[gnu::packed]] {
		uint16_t es;
		uint16_t ds;
		uint16_t di;
		uint16_t si;
		uint16_t bp;
		uint16_t bx;
		uint16_t dx;
		uint16_t cx;
		uint16_t ax;
		uint16_t f;
		uint16_t ph1;
		uint16_t ph2;
		uint16_t ip;
		uint16_t cs;
		uint16_t rf;
		uint8_t rettype;
	} regs;
	uint8_t data[sizeof(regs)];
};

struct ENTRY_STATE{
	uint8_t entry;
	uint8_t irq_no;
	IRQ_REGS regs;
};

struct BuffList
{
	uint8_t * buff;
	ssize_t len;
	BuffList * next;
};

struct CMD {
	volatile uint8_t command;
	volatile BuffList * send;
	volatile BuffList * recv;
};

struct [[gnu::packed]] STACKCODE8{
	struct [[gnu::packed]] {
		uint16_t es;
		uint16_t ds;
		uint16_t di;
		uint16_t si;
		uint16_t bx;
		uint16_t dx;
		uint16_t cx;
		uint16_t ax;
		uint16_t f;
	} regs;
	uint8_t code[8];
};

struct Thread_SHM : public StaticThread<1024>
{
	ENTRY_STATE entry;
	CMD cmd;
	void wait_for_exec()
	{
		while(cmd.command)
			yield();
	}
	void stackcode8(STACKCODE8 * param)
	{
		BuffList recv = {
				reinterpret_cast<uint8_t*>(param),
				sizeof(param->regs),
				nullptr
		};
		BuffList send = {
				reinterpret_cast<uint8_t*>(param),
				sizeof(*param),
				nullptr
		};
		{
			cmd.send = &send;
			cmd.recv = &recv;
			cmd.command = 0x01;
			wait_for_exec();
		}
	}
	void set_return()
	{
		cmd.recv = nullptr;
		BuffList send = {
				entry.regs.data,
				sizeof(entry.regs.data),
				nullptr
		};
		cmd.send = &send;
		cmd.command = 0x02;
		wait_for_exec();
	}
	void chain(uint32_t chain)
	{
		entry.regs.regs.ph1 = chain;
		entry.regs.regs.ph2 = chain>>16;
		entry.regs.regs.rettype = 0x00;
	}
	void callback_end()
	{
		cmd.recv = nullptr;
		cmd.send = nullptr;
		cmd.command = 0x03;
		while(1)
			yield();
	}
	void putstr(const char * str)
	{
		STACKCODE8 regs = {
				.regs = {
				.es = 0x00,
				.ds = 0x00,
				.di = 0x00,
				.si = 0x00,
				.bx = 0x00,
				.dx = 0x00,
				.cx = 0x00,
				.ax = 0x00,
				.f = 0x00
				},
				.code = { 0xCD, 0x10, 0xCB }
		};
		regs.regs.f = entry.regs.regs.f;

		while(*str)
		{
			regs.regs.ax = (0x0e<<8)|*str++;
			stackcode8(&regs);
		}
	}

	uint32_t install_irq(uint8_t irq)
	{
		uint32_t ret;
		BuffList recv = {
				reinterpret_cast<uint8_t*>(&ret),
				sizeof(ret),
				nullptr
		};
		BuffList send = {
				reinterpret_cast<uint8_t*>(&irq),
				sizeof(irq),
				nullptr
		};
		{
			cmd.recv = &recv;
			cmd.send = &send;
			cmd.command = 0x08;
			wait_for_exec();
		}
		return ret;
	}

	char getch() {
		STACKCODE8 regs = {
				.regs = {
				.es = 0x00,
				.ds = 0x00,
				.di = 0x00,
				.si = 0x00,
				.bx = 0x00,
				.dx = 0x00,
				.cx = 0x00,
				.ax = 0x00,
				.f = 0x00
				},
				.code = { 0xCD, 0x16, 0xCB }
		};
		regs.regs.f = entry.regs.regs.f;
		regs.regs.ax = 0x0100;
		stackcode8(&regs);
		return regs.regs.ax;
	}
};

#define OROMHandler_type_section [[gnu::section("or_handlers"),gnu::used]] const
struct OROMHandler {
	bool (*decide)(const ENTRY_STATE &);
	void (*entry)(Thread_SHM *);
};

void monitor_install(Thread * main);
void int19_install(Thread * main);
