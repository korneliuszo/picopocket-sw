#include "uart_tcp_server.hpp"

#ifdef PICOPOCKET_SIM
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <cerrno>
#else
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#endif

#ifdef PICOPOCKET_SIM
#else

void LWIP_TCP_16550::try_send(struct tcp_pcb *tpcb)
{
	LWIP_TCP_16550 * arg = reinterpret_cast<LWIP_TCP_16550 *>(tpcb->callback_arg);
	err_t wr_err = ERR_OK;
	for(auto buff=arg->send_buffer();
		(wr_err == ERR_OK) && buff.get_len();
		buff=arg->send_buffer())
	{
		size_t len = buff.stop-buff.start;
		wr_err = tcp_write(tpcb, const_cast<uint8_t*>(buff.start),len,1);

		if(wr_err == ERR_OK)
		{
			arg->send_commit(len);
		}
	}
	if(wr_err == ERR_OK || wr_err == ERR_MEM)
		wr_err = tcp_output(tpcb);
}


void LWIP_TCP_16550::try_recv(struct tcp_pcb *tpcb)
{
	LWIP_TCP_16550 * arg = reinterpret_cast<LWIP_TCP_16550 *>(tpcb->callback_arg);

	for(auto buff=arg->recv_buffer();
		arg->recv_pbuff && buff.get_len();
		buff=arg->recv_buffer())
	{
		size_t len = std::min(buff.get_len(),(arg->recv_pbuff->len-arg->off));
		buff.put_bytes(&static_cast<uint8_t*>(arg->recv_pbuff->payload)[arg->off],len,0);
		arg->recv_commit(len);
		arg->off+=len;
		if(arg->off == arg->recv_pbuff->len)
		{
			auto next = arg->recv_pbuff->next;
			if(next)
				pbuf_ref(next);
			else
				arg->rx_flush();
			pbuf_free(arg->recv_pbuff);
			arg->recv_pbuff = next;
			tcp_recved(tpcb, arg->off);
			arg->off = 0;
		}
	}
}

void LWIP_TCP_16550::error_fn(void *arg, err_t err)
{
	LWIP_TCP_16550 * aarg = reinterpret_cast<LWIP_TCP_16550 *>(arg);
	aarg->csock = nullptr;
	if(aarg->recv_pbuff)
		pbuf_free(aarg->recv_pbuff);
	aarg->recv_pbuff=nullptr;
}

void LWIP_TCP_16550::sock_close(struct tcp_pcb *tpcb)
{
	error_fn(tpcb->callback_arg,ERR_OK);
	tcp_arg(tpcb, NULL);
	tcp_recv(tpcb, NULL);
	tcp_err(tpcb, NULL);
	tcp_poll(tpcb, NULL, 0);

	tcp_close(tpcb);
}

err_t LWIP_TCP_16550::recv_fn(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	LWIP_TCP_16550 * aarg = reinterpret_cast<LWIP_TCP_16550 *>(tpcb->callback_arg);
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
		error_fn(arg, err);
		return err;
	}
	if(aarg->recv_pbuff)
	    pbuf_cat(aarg->recv_pbuff,p);
	else
		aarg->recv_pbuff = p;
	try_recv(tpcb);
    return ERR_OK;
}


err_t LWIP_TCP_16550::accept_fn(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	LWIP_TCP_16550 * aarg = reinterpret_cast<LWIP_TCP_16550 *>(arg);
	if ((err != ERR_OK) || (newpcb == NULL)) {
		return ERR_VAL;
	}

	if(aarg->csock)
	{
		return ERR_MEM;
	}
	aarg->csock = newpcb;

	aarg->off = 0;
	tcp_arg(newpcb, arg);
	tcp_recv(newpcb, recv_fn);
	tcp_err(newpcb, error_fn);
	return ERR_OK;
}

#endif


void LWIP_TCP_16550::connect(uint16_t port, uint8_t irq, uint16_t tcpport)
{
	UART_16550::connect(port,irq);
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
    servaddr.sin_port = htons(tcpport);
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
    tcp_bind(lsock,IP_ANY_TYPE,tcpport);
    tcp_arg(lsock,this);
    tcp_accept(tcp_listen(lsock),accept_fn);
    cyw43_arch_lwip_end();
#endif
}

void LWIP_TCP_16550::poll()
{
	UART_16550::poll();
#ifndef PICOPOCKET_SIM
	if(csock)
	{
		cyw43_arch_lwip_begin();
		try_recv(csock);
		try_send(csock);
		cyw43_arch_lwip_end();
	}
#else

	if(!csock)
	{
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
			auto buff = UART_16550::recv_buffer();
			int len = recv(csock, (void*) buff.start,
					buff.stop - buff.start,
					0);
			if (len == -1 && (errno != EAGAIN && errno != EWOULDBLOCK))
			{
				close(csock);
				csock = 0;
				return;
			}
			if (len == 0 && buff.stop - buff.start !=0)
			{
				close(csock);
				csock = 0;
				return;
			}
			if (len > 0)
				UART_16550::recv_commit(len);
			else
			{
				UART_16550::rx_flush();
			}
		}
		for(auto buff=UART_16550::send_buffer();
			buff.get_len();
			buff=UART_16550::send_buffer())
		{
			int len = send(csock, (void*) buff.start,
					buff.stop - buff.start,
					0);
			if (len < 0)
			{
				close(csock);
				csock = 0;
				return;
			}
			if (len > 0)
				UART_16550::send_commit(len);
		}
	}

#endif
}

