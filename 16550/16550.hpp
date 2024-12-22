#pragma once

#include "fifo.hpp"
#include "fifo_with_sep.hpp"
#include "stdint.h"

class UART_16550
{
	FIFO_WITH_BRK<uint8_t,16> RX;
	Fifo<uint8_t,16> TX;

	uint8_t irqh = 0xff;

	uint8_t DLL;
	uint8_t DLM;
	union {
		struct {
			uint8_t ERBFI:1;
			uint8_t ETBEI:1;
			uint8_t ELSI:1;
			uint8_t EDSSI:1;
		} b;
		uint8_t v;
	} IER;

	enum class IRQ_Type : uint8_t {
		NONE= 1, //No interrupt
		LSR = 6, //Receiver Line Status
		RDA = 4, //Received Data Available
		CTO = 12, //Character Timeout Indication
		THRE= 2, //Transmitter Holding Register Empty
		MDMS= 0, // MODEM Status
	};

	union IIR_t{
		struct {
			IRQ_Type IRQT:4;
			uint8_t ZEROS:2;
			uint8_t FIFOEN:2;
		} b;
		uint8_t v;
	};

	union {
		struct {
			uint8_t FIFOEN:1;
			uint8_t RXFIFORST:1;
			uint8_t TXFIFORST:1;
			uint8_t DMAMODE:1; //UNUSED
			uint8_t RSVD:2;
			uint8_t RCVR_TRIG:2;
		} b;
		uint8_t v;
	} volatile FCR;

	union {
		struct {
			uint8_t WLS:2;
			uint8_t STB:1;
			uint8_t PEN:1;
			uint8_t EPS:1;
			uint8_t SPAR:1;
			uint8_t SB:1;
			uint8_t DLAB:1;
		} b;
		uint8_t v;
	} LCR;

	union {
		struct {
			uint8_t DTR:1;
			uint8_t RTS:1;
			uint8_t OUT1:1;
			uint8_t OUT2:1;
			uint8_t LOOP:1;
			uint8_t ZERO:3;
		} b;
		uint8_t v;
	} MCR;

	union LSR_t {
		struct {
			uint8_t DR:1;
			uint8_t OE:1;
			uint8_t PE:1;
			uint8_t FE:1;
			uint8_t BI:1;
			uint8_t THRE:1;
			uint8_t TEMT:1;
			uint8_t EIRF:1;
		} b;
		uint8_t v;
	};

	uint8_t SCR;
	volatile bool RLS_IRQ;
	volatile bool CTO_IRQ;
	volatile bool THRE_IRQ;
	volatile bool MODEM_IRQ;

	volatile bool BI_LATCH;

	volatile bool CTS;
	bool DCTS;
	volatile bool DSR;
	bool DDSR;

	union MSR_t {
		struct {
			uint8_t DCTS:1;
			uint8_t DDSR:1;
			uint8_t TERI:1;
			uint8_t DDCD:1;
			uint8_t CTS:1;
			uint8_t DSR:1;
			uint8_t RI:1;
			uint8_t DCD:1;
		} b;
		uint8_t v;
	};

	inline long rx_bytes_trigger() volatile
	{
		static const long lvl_tbl[4] =
		{
				1,4,8,14
		};
		return lvl_tbl[FCR.b.RCVR_TRIG];
	}

	uint32_t io_rdfn(uint16_t faddr);
	void io_wrfn(uint32_t faddr, uint8_t data);

	void update_irq();

public:
	UART_16550()
	{};

	void connect(uint16_t port, uint8_t irq);

	void poll();

	inline auto send_buffer()
	{
		return TX.get_rdbuff();
	}
	inline void send_commit(size_t len)
	{
		TX.commit_rdbuff(len);

		if(TX.fifo_free())
		{
			THRE_IRQ = true;
			update_irq();
		}
	}
	inline auto recv_buffer()
	{
		return RX.get_wrbuff();
	}

	inline void recv_commit(size_t len)
	{
		RX.commit_wrbuff(len);
		if(RX.fifo_check() >= rx_bytes_trigger() && IER.b.ERBFI)
		{
			update_irq();
		}
	}

	inline void recv_put_brk()
	{
		RX.fifo_put_brk();
		if(RX.is_brk_on_top())
		{
			BI_LATCH = true;
			RLS_IRQ = true;
			update_irq();
		}
	}

	inline void rx_flush()
	{
		if(!CTO_IRQ && RX.fifo_check())
		{
			CTO_IRQ = true;
			update_irq();
		}
	}

	void update_control_lines(bool cts, bool dsr, bool from_loopback=false)
	{
		if(!MCR.b.LOOP && !from_loopback)
			return;
		bool changed = false;
		if(cts != CTS)
			changed = true;
		CTS = cts;
		if(dsr != DSR)
			changed = true;
		DSR = dsr;
		if(changed)
		{
			MODEM_IRQ = true;
			update_irq();
		}
	}
};

