#include "or_internal.hpp"
#include "config.hpp"
#include "fifo.hpp"
#include <cassert>

#ifdef PICOPOCKET_SIM
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#else
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/tcp.h"
#endif
#include <atomic>

#ifdef PICOPOCKET_SIM
using PCB = int;
#else
using PCB = tcp_pcb*;
#endif

struct Recv_state {
	Fifo<uint8_t,2048> recvbuff;
	Fifo<uint8_t,2048> sendbuff;
	std::atomic<bool> sig_rst;
};

static volatile PCB lsock, csock;
static Recv_state crstate;



#ifdef PICOPOCKET_SIM
#else

static void
try_send(struct tcp_pcb *tpcb)
{
	err_t wr_err = ERR_OK;
	while((wr_err == ERR_OK) && crstate.sendbuff.fifo_check())
	{
		uint16_t len;
		{
			auto buff = crstate.sendbuff.get_rdbuff();
			len = buff.stop-buff.start;
			wr_err = tcp_write(tpcb, const_cast<uint8_t*>(buff.start),len,1);
		}
		if(wr_err == ERR_OK)
		{
			crstate.sendbuff.commit_rdbuff(len);
			if(!crstate.sendbuff.fifo_check())
				wr_err = tcp_output(tpcb);
		}
	}
}

static struct pbuf * volatile recv_pbuff;
static size_t off = 0;

static
void try_recv(struct tcp_pcb *tpcb)
{
	while(recv_pbuff && crstate.recvbuff.fifo_free())
	{
		auto buff = crstate.recvbuff.get_wrbuff();
		size_t len = std::min(crstate.recvbuff.fifo_free(),(recv_pbuff->len-off));
		buff.put_bytes(&static_cast<uint8_t*>(recv_pbuff->payload)[off],len,0);
		crstate.recvbuff.commit_wrbuff(len);
		off+=len;
		if(off == recv_pbuff->len)
		{
			auto next = recv_pbuff->next;
			if(next)
				pbuf_ref(next);
			pbuf_free(recv_pbuff);
			recv_pbuff = next;
			tcp_recved(tpcb, off);
			off = 0;
		}
	}
}

static void
error_fn(void *arg, err_t err)
{
	crstate.sig_rst = true;
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
sent_fn(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	if(!csock)
		sock_close(tpcb);
	return ERR_OK;
}

static err_t
poll_fn(void *arg, struct tcp_pcb *tpcb)
{
	if(!csock)
		sock_close(tpcb);
	return ERR_OK;
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
	try_recv(tpcb);
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
	off = 0;
	tcp_arg(nullptr, csock);
	tcp_recv(newpcb, recv_fn);
	tcp_err(newpcb, error_fn);
	tcp_poll(newpcb, poll_fn, 0);
	tcp_sent(newpcb, sent_fn);
	return ERR_OK;
}

#endif

void monitor_install(Thread * main)
{
#ifdef PICOPOCKET_SIM
	lsock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (lsock < 0)
		exit(1);

	int reuse = 1;
	if(setsockopt(lsock,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))!=0)
		exit(1);
	struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(5555);
    if ((bind(lsock, (sockaddr*)&servaddr, sizeof(servaddr))) != 0)
		exit(1);
    if ((listen(lsock, 2)) != 0) {
		exit(1);
	//if (fcntl(lsock, F_SETFL, fcntl(lsock, F_GETFL, 0) | O_NONBLOCK) == -1)
	//	exit(1);
    }
#else
	cyw43_arch_lwip_begin();
    lsock = tcp_new();
    tcp_bind(lsock,IP_ANY_TYPE,5555);
    tcp_accept(tcp_listen(lsock),accept_fn);
    cyw43_arch_lwip_end();
#endif
}


static bool monitor_callback;

static bool monitor_decide(const volatile ENTRY_STATE & state)
{
	if(state.entry == 0) //boot
		return false;
	if(state.entry == 1 && state.irq_no == 0x19)
		return true;
	if(state.entry == 2)
		return true;
	return monitor_callback;
}

static void monitor_entry (Thread_SHM * thread)
{
	if(thread->params.irq_no == 0x19 || thread->params.entry == 2)
	{
		if(thread->params.regs.regs.rettype& 0x80)
			thread->callback_end();
		bool autoboot = Config::MONITOR_AUTOBOOT::val.ival;
		thread->putstr("PicoPocket monitor: press M to ");
		if(autoboot)
			thread->putstr("stop ");
		thread->putstr("entry\r\n");
		monitor_callback = (thread->getch() == 'm') ^ autoboot;
		if(!monitor_callback)
			thread->callback_end(); //nonreturn
		thread->putstr("Monitor entry\r\n");
	}
	while(1)
	{
		restart:
		while(crstate.recvbuff.fifo_check()<9)
		{
			if(crstate.sig_rst)
			{
				crstate.recvbuff.clear_from_rd();
				crstate.sig_rst = false;
				goto restart;
			}
			thread->yield();
		}
		volatile uint32_t slen,rlen;
		volatile uint8_t cmd;
		{
			auto lenbuff = crstate.recvbuff.get_rdbuff();
			slen = *lenbuff[0] | (*lenbuff[1]<<8) | (*lenbuff[2]<<16) | (*lenbuff[3]<<24);
			rlen = *lenbuff[4] | (*lenbuff[5]<<8) | (*lenbuff[6]<<16) | (*lenbuff[7]<<24);
			cmd = *lenbuff[8];
		}
		crstate.recvbuff.commit_rdbuff(9);
		while(crstate.recvbuff.fifo_check()<slen)
		{
			if(crstate.sig_rst)
			{
				crstate.recvbuff.clear_from_rd();
				crstate.sig_rst = false;
				goto restart;
			}
			thread->yield();
		}
		while(crstate.sendbuff.fifo_free()<rlen)
			thread->yield();
		switch(cmd)
		{
		default:
		 {
			auto rbuff = crstate.recvbuff.get_rdbuff();
			rbuff.set_len(slen);
			auto wbuff = crstate.sendbuff.get_wrbuff();
			wbuff.set_len(rlen);
			BuffList req[4] = {
					{
						const_cast<uint8_t*>(rbuff.start),
						rbuff.stop - rbuff.start,
						&req[1]
					},
					{
						const_cast<uint8_t*>(rbuff.start2),
						rbuff.stop2 - rbuff.start2,
						nullptr
					},
					{
						const_cast<uint8_t*>(wbuff.start),
						wbuff.stop - wbuff.start,
						&req[3]
					},
					{
						const_cast<uint8_t*>(wbuff.start2),
						wbuff.stop2 - wbuff.start2,
						nullptr
					},
			};
			if(!req[1].len)
				req[0].next = nullptr;
			if(!req[3].len)
				req[2].next = nullptr;

			{
				if (cmd == 0x03)
				{
					crstate.recvbuff.commit_rdbuff(slen);
					crstate.sendbuff.commit_wrbuff(rlen);
					thread->callback_end_noset();
				}
				thread->cmd.send = req[0].len ? &req[0] : nullptr;
				thread->cmd.recv = req[2].len ? &req[2] : nullptr;
				thread->cmd.command = cmd;
				thread->wait_for_exec();
			}
		  }
		 break;
		}
		crstate.recvbuff.commit_rdbuff(slen);
		crstate.sendbuff.commit_wrbuff(rlen);
	}

	thread->callback_end();

}

void monitor_poll()
{
#ifndef PICOPOCKET_SIM
	if(csock)
	{
		cyw43_arch_lwip_begin();
		try_recv(csock);
		try_send(csock);
		cyw43_arch_lwip_end();
	}
	else
	{
		crstate.sendbuff.clear_from_rd();
	}
#else

	if(!csock)
	{
		crstate.sendbuff.clear_from_rd();
		PCB connfd = accept4(lsock, nullptr, nullptr, SOCK_NONBLOCK);
		if(connfd > 0)
		{
			csock = connfd;
			int nodelay = 1;
			if(setsockopt(csock,IPPROTO_TCP,TCP_NODELAY,&nodelay,sizeof(nodelay))!=0)
				exit(1);
		}
	}
	if(csock)
	{
		{
			auto buff = crstate.recvbuff.get_wrbuff();
			int len = recv(csock, (void*) buff.start,
					buff.stop - buff.start,
					0);
			if (len == -1 && (errno != EAGAIN && errno != EWOULDBLOCK))
			{
				close(csock);
				csock = 0;
				crstate.sig_rst = true;
				return;
			}
			if (len == 0 && buff.stop - buff.start !=0)
			{
				close(csock);
				csock = 0;
				crstate.sig_rst = true;
				return;
			}
			if (len > 0)
				crstate.recvbuff.commit_wrbuff(len);
		}
		if(crstate.sendbuff.fifo_check()){
			auto buff = crstate.sendbuff.get_rdbuff();
			int len = send(csock, (void*) buff.start,
					buff.stop - buff.start,
					0);
			if (len < 0)
			{
				close(csock);
				csock = 0;
				crstate.sig_rst = true;
				return;
			}
			if (len > 0)
				crstate.sendbuff.commit_rdbuff(len);
		}
	}

#endif
}

const OROMHandler monitor_handler = {
		monitor_decide,
		monitor_entry
};
