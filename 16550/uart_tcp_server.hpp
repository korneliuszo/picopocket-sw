#pragma once

#include "16550.hpp"
#ifndef PICOPOCKET_SIM
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#endif
#include <atomic>


class LWIP_TCP_16550 : private UART_16550 {

#ifdef PICOPOCKET_SIM
using PCB = int;
#else
using PCB = tcp_pcb*;
#endif

#ifndef PICOPOCKET_SIM
	static void try_send(struct tcp_pcb *tpcb);
	static void try_recv(struct tcp_pcb *tpcb);

	static void error_fn(void *arg, err_t err);
	static void sock_close(struct tcp_pcb *tpcb);
	static err_t recv_fn(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
	static err_t accept_fn(void *arg, struct tcp_pcb *newpcb, err_t err);

	struct pbuf * volatile recv_pbuff;
	size_t off = 0;
#endif

	PCB volatile lsock, csock;
public:
	void connect(uint16_t port, uint8_t irq, uint16_t tcpport);
	void poll();

};
