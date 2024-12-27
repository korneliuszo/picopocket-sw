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
	CMD cmd;
	volatile ENTRY_STATE params;
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
	void putmem(uint16_t seg, uint16_t addr, uint8_t* data, uint16_t len)
	{
		uint8_t param[] = {
				(uint8_t)(seg>>8),
				(uint8_t)(seg>>0),
				(uint8_t)(addr>>8),
				(uint8_t)(addr>>0),
				(uint8_t)(len>>8),
				(uint8_t)(len>>0),
		};		BuffList send[] = {
				{
						param,
						sizeof(param),
						&send[1]
				},
				{
						data,
						len,
						nullptr
				}
		};
		{
			cmd.send = send;
			cmd.recv = nullptr;
			cmd.command = 0x06;
			wait_for_exec();
		}
	}
	void getmem(uint16_t seg, uint16_t addr, uint8_t* data, uint16_t len)
		{
		uint8_t param[] = {
				(uint8_t)(seg>>8),
				(uint8_t)(seg>>0),
				(uint8_t)(addr>>8),
				(uint8_t)(addr>>0),
				(uint8_t)(len>>8),
				(uint8_t)(len>>0),
		};
		BuffList recv = {
				data,
				len,
				nullptr
		};
		BuffList send = {
				param,
				sizeof(param),
				nullptr
		};
		{
			cmd.send = &send;
			cmd.recv = &recv;
			cmd.command = 0x07;
			wait_for_exec();
		}
	}
private:
	void set_return(volatile IRQ_REGS& entry)
	{
		cmd.recv = nullptr;
		BuffList send = {
				const_cast<uint8_t*>(entry.data),
				sizeof(entry.data),
				nullptr
		};
		cmd.send = &send;
		cmd.command = 0x02;
		wait_for_exec();
	}
public:
	void chain(uint32_t chain)
	{
		params.regs.regs.ph1 = chain;
		params.regs.regs.ph2 = chain>>16;
		params.regs.regs.rettype = 0x00;
	}
	void callback_end()
	{
		set_return(params.regs);
		callback_end_noset();
	}
	void callback_end_noset()
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
		regs.regs.f = params.regs.regs.f;

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
		regs.regs.f = params.regs.regs.f;
		regs.regs.ax = 0x0100;
		stackcode8(&regs);
		return regs.regs.ax;
	}
	/*
	ENTRY_STATE get_entry()
	{
		ENTRY_STATE ret;
		BuffList recv[] = {
				{
						reinterpret_cast<uint8_t*>(&ret.entry),
						sizeof(ret.entry),
						&recv[1]
				},
				{
						reinterpret_cast<uint8_t*>(&ret.irq_no),
						sizeof(ret.irq_no),
						&recv[2]
				},
				{
						reinterpret_cast<uint8_t*>(&ret.regs.regs),
						sizeof(ret.regs.regs),
						nullptr
				},
		};
		{
			cmd.recv = &recv[0];
			cmd.send = nullptr;
			cmd.command = 0x0A;
			wait_for_exec();
		}
		return ret;
	}
	*/

};

struct OROMHandler {
	bool (*decide)(const volatile ENTRY_STATE &);
	void (*entry)(Thread_SHM *);
};

void monitor_install(Thread * main);
void monitor_poll();
extern const OROMHandler monitor_handler;

void int19_install(Thread * main);
extern const OROMHandler int19_handler;

void ramdisk_install(Thread * main);
extern const OROMHandler ramdisk_handler;

void config_install(Thread * main);
extern const OROMHandler config_handler;

void disk_mapper_install(Thread * main);
extern const OROMHandler disk_mapper_handler;
