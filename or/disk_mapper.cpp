#include "or_internal.hpp"
#include "config.hpp"
#include "fifo.hpp"
#include "int13_handler.hpp"
#include <cassert>

#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/tcp.h"

#include <atomic>

using PCB = tcp_pcb*;
static PCB lsock, csock;
static struct pbuf * recv_pbuff;
static bool first_recv;

struct MAP {
	uint8_t from;
	uint8_t to;
	uint16_t C;
	uint8_t H;
	uint8_t S;
	uint32_t LBA;
};
static std::array<MAP,10> disk_map;
static int8_t disks_no_off;


static void
error_fn(void *arg, err_t err)
{
	csock = nullptr;
	if(recv_pbuff)
		pbuf_free(recv_pbuff);
	recv_pbuff=nullptr;
}

static void
sock_close(struct tcp_pcb *tpcb)
{
	tcp_arg(tpcb, NULL);
	tcp_sent(tpcb, NULL);
	tcp_recv(tpcb, NULL);
	tcp_err(tpcb, NULL);
	tcp_poll(tpcb, NULL, 0);
	error_fn(nullptr,ERR_OK);

	tcp_close(tpcb);
}

static err_t
recv_fn(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	if (p == NULL)
	{
	    /* remote host closed connection */
		sock_close(tpcb);
		return ERR_OK;
	}
	if(err != ERR_OK) {
		/* cleanup, for unknown reason */
		if (p != NULL) {
			pbuf_free(p);
		}
		error_fn(nullptr, err);
		return err;
	}
	if(recv_pbuff)
	    pbuf_cat(recv_pbuff,p);
	else
		recv_pbuff = p;

	constexpr auto FIRST_RX_LEN = (
			1+disk_map.size()*(10)
	);

	if(first_recv && recv_pbuff->tot_len >= FIRST_RX_LEN)
	{
		pbuf_copy_partial(recv_pbuff,&disks_no_off,sizeof(disks_no_off),0);
		for(auto i=0;i<disk_map.size();i++)
		{
			pbuf_copy_partial(recv_pbuff,&disk_map[i].from,
					1,1+i*10+0
					);
			pbuf_copy_partial(recv_pbuff,&disk_map[i].to,
					1,1+i*10+1
					);
			pbuf_copy_partial(recv_pbuff,&disk_map[i].C,
					2,1+i*10+2
					);
			pbuf_copy_partial(recv_pbuff,&disk_map[i].H,
					1,1+i*10+4
					);
			pbuf_copy_partial(recv_pbuff,&disk_map[i].S,
					1,1+i*10+5
					);
			pbuf_copy_partial(recv_pbuff,&disk_map[i].LBA,
					4,1+i*10+6
					);
		}
		recv_pbuff = pbuf_free_header(recv_pbuff,FIRST_RX_LEN);
		tcp_recved(tpcb,FIRST_RX_LEN);
		first_recv = false;
	}

    return ERR_OK;
}

static err_t accept_fn(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	LWIP_UNUSED_ARG(arg);
	if ((err != ERR_OK) || (newpcb == NULL)) {
		return ERR_VAL;
	}

	if(csock)
	{
		return ERR_MEM;
	}
	csock = newpcb;

	for(auto & i: disk_map)
		i.from = 0xFF;
	first_recv = true;

	tcp_arg(nullptr, csock);
	tcp_recv(newpcb, recv_fn);
	tcp_err(newpcb, error_fn);
	//tcp_poll(newpcb, poll_fn, 0);
	//tcp_sent(newpcb, sent_fn);
	return ERR_OK;
}

void disk_mapper_install(Thread * main)
{

	for(auto & i: disk_map)
		i.from = 0xFF;
	first_recv = true;

	cyw43_arch_lwip_begin();
	lsock = tcp_new();
	tcp_bind(lsock,IP_ANY_TYPE,5557);
	tcp_accept(tcp_listen(lsock),accept_fn);
	cyw43_arch_lwip_end();
}

static bool disk_manager_callback;

static bool disk_mapper_decide(const ENTRY_STATE & state)
{
	if(state.entry == 0) //boot
		return false;
	if(state.entry == 1 && state.irq_no == 0x19)
		return true;
	if(disk_manager_callback && state.entry == 1 && state.irq_no == 0x13)
		return true;
	return false;
}

class Mapper_read_completion {
	uint8_t * sector;
public:
	Mapper_read_completion(uint8_t * _sector):sector(_sector){};
	bool complete_trigger()
	{
		cyw43_arch_lwip_begin();
		if(first_recv || !recv_pbuff || recv_pbuff->tot_len < 512)
		{
			cyw43_arch_lwip_end();
			return false;
		}
		pbuf_copy_partial(recv_pbuff,sector,512,0);
		recv_pbuff = pbuf_free_header(recv_pbuff,512);
		tcp_recved(csock,512);
		cyw43_arch_lwip_end();
		return true;
	}
};

auto mapper_read(Thread_SHM * thread, uint8_t drive_no, uint32_t LBA,uint8_t sector[512])
{
	while(!csock)
		thread->yield();
	struct [[gnu::packed]] {
		uint8_t type;
		uint8_t drive_no;
		uint32_t lba;
	} header =
	{
		0x01,
		drive_no,
		LBA
	};
	cyw43_arch_lwip_begin();
	if (tcp_write(csock,&header,sizeof(header),1) != ERR_OK)
	{
		sock_close(csock);
	}
	if (tcp_output(csock) != ERR_OK)
	{
		sock_close(csock);
	}
	cyw43_arch_lwip_end();
	return Mapper_read_completion(sector);
}

class Mapper_write_completion {
public:
	bool complete_trigger()
	{
		cyw43_arch_lwip_begin();
		if(first_recv || !recv_pbuff || recv_pbuff->tot_len < 1)
		{
			cyw43_arch_lwip_end();
			return false;
		}
		recv_pbuff = pbuf_free_header(recv_pbuff,1);
		tcp_recved(csock,1);
		cyw43_arch_lwip_end();
		return true;
	}
};

auto mapper_write(Thread_SHM * thread, uint8_t drive_no, uint32_t LBA,uint8_t sector[512])
{
	while(!csock)
		thread->yield();
	struct [[gnu::packed]] {
		uint8_t type;
		uint8_t drive_no;
		uint32_t lba;
	} header =
	{
		0x02,
		drive_no,
		LBA
	};
	cyw43_arch_lwip_begin();
	if (tcp_write(csock,&header,sizeof(header),1) != ERR_OK)
	{
		sock_close(csock);
	}
	if (tcp_write(csock,sector,512,0) != ERR_OK)
	{
		sock_close(csock);
	}
	if (tcp_output(csock) != ERR_OK)
	{
		sock_close(csock);
	}
	cyw43_arch_lwip_end();
	return Mapper_write_completion();
}


static uint32_t int13chain = 0;

static void disk_mapper_entry (Thread_SHM * thread)
{
	auto entry = thread->get_entry();
	if(entry.irq_no == 0x19)
	{
		if(entry.regs.regs.rettype& 0x80)
			thread->callback_end();
		bool autoboot = true;//Config::MONITOR_AUTOBOOT::val.ival;
		thread->putstr("PicoPocket disk mapper: press D to ");
		if(autoboot)
			thread->putstr("stop ");
		thread->putstr("entry\r\n");
		disk_manager_callback = (thread->getch() == 'd') ^ autoboot;
		if(!disk_manager_callback)
			thread->callback_end(); //nonreturn

		int13chain = thread->install_irq(0x13);
		while(first_recv)
			thread->yield();

		uint8_t disks;
		thread->getmem(0,0x475,&disks,1);
		disks+=disks_no_off;
		thread->putmem(0,0x475,&disks,1);

		thread->putstr("Mapper installed\r\n");
		thread->callback_end(); //nonreturn
	}

	if(int13chain)
		thread->chain(int13chain);

	auto params = thread->get_entry();
	for(auto map:disk_map)
	{
		if((params.regs.regs.dx & 0xff) == map.from)
		{
			if(map.to == 0xff)
			{
				int13_handle<mapper_read,mapper_write>(thread,map.from,
						map.C,map.H,map.S,map.LBA);
			}
			else
			{
				params.regs.regs.dx = (params.regs.regs.dx & 0xff00) | map.to;
				thread->set_return(params.regs);
			}
			break;
		}
	}
	thread->callback_end(); //nonreturn
}

const OROMHandler disk_mapper_handler = {
		disk_mapper_decide,
		disk_mapper_entry
};
