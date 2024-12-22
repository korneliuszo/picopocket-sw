#include "16550.hpp"
#include "isa_worker.hpp"
#include <stdio.h>

template
<typename R, class C, auto F, typename ... Args>
static R CWrap(void * v_obj, Args ... args)
{
	C * that = reinterpret_cast<C*>(v_obj);
	return (that->*F)(args...);
};

uint32_t UART_16550::io_rdfn(uint16_t faddr)
{
	switch(faddr)
	{
	case 0:
		if(LCR.b.DLAB)
		{ //DLL
			return DLL;
		}
		else
		{ //RBR
			auto rxbuff = RX.get_rdbuff();
			if(rxbuff.get_len())
			{
				uint8_t data = rxbuff.start[0];
				if(RX.is_brk_on_top())
				{
					BI_LATCH = true;
					RLS_IRQ = true;
				}
				RX.commit_rdbuff(1);
				CTO_IRQ = false;
				update_irq();
				return data;
			}
			else return 0xff;
		}
	case 1:
		if(LCR.b.DLAB)
		{ //DLM
			return DLM;
		}
		else
		{ //IER
			return IER.v;
		}
	case 2:
	{
		IIR_t IIR = {.v = 0};
		IIR.b.FIFOEN = (FCR.b.FIFOEN)? 3 : 0;
		if(RLS_IRQ && IER.b.ELSI)
		{
			IIR.b.IRQT = IRQ_Type::LSR;
		}
		else if ((RX.fifo_check() >= rx_bytes_trigger())  && IER.b.ERBFI)
		{
			IIR.b.IRQT = IRQ_Type::RDA;
		}
		else if (CTO_IRQ  && IER.b.ERBFI)
		{
			IIR.b.IRQT = IRQ_Type::CTO;
		}
		else if (THRE_IRQ && IER.b.ETBEI)
		{
			THRE_IRQ = false;
			IIR.b.IRQT = IRQ_Type::THRE;
			update_irq();
		}
		else if (MODEM_IRQ  && IER.b.EDSSI)
		{
			IIR.b.IRQT = IRQ_Type::MDMS;
		}
		else
		{
			IIR.b.IRQT = IRQ_Type::NONE;
		}
		return IIR.v;
	}
	case 3:
		return LCR.v;
	case 4:
		return MCR.v;
	case 5:
	{
		LSR_t LSR = {.v = 0};
		RLS_IRQ = false;
		update_irq();
		LSR.b.DR = !!(RX.fifo_check());
		LSR.b.BI = BI_LATCH;
		BI_LATCH = false;
		LSR.b.EIRF = RX.is_brk_in_queue();
		LSR.b.THRE = MCR.b.LOOP ? (!!(RX.fifo_free())) : (!!(TX.fifo_free()));
		LSR.b.TEMT = (TX.fifo_check()==0);
		return LSR.v;
	}
	case 6:
	{
		MSR_t MSR = {.v=0};
		MSR.b.CTS = CTS;
		MSR.b.DSR = DSR;
		MSR.b.DCTS = CTS ^ DCTS;
		DCTS = CTS;
		MSR.b.DDSR = DSR ^ DDSR;
		DDSR = DSR;
		MODEM_IRQ = false;
		update_irq();
		return MSR.v;
	}
	case 7:
		return SCR;
	default:
		return 0xff;
	};
}

void UART_16550::io_wrfn(uint32_t faddr, uint8_t data)
{
	switch (faddr)
	{
	case 0:
		if(LCR.b.DLAB)
		{ //DLL
			DLL = data;
		}
		else
		{
			THRE_IRQ = false;
			update_irq();
			if(MCR.b.LOOP)
			{
				if(RX.fifo_free())
				{
					auto buff = RX.get_wrbuff();
					buff.start[0] = data;
					RX.commit_wrbuff(1);
				}
			}
			else
			{
				if(TX.fifo_free())
				{
					auto buff = TX.get_wrbuff();
					buff.start[0] = data;
					TX.commit_wrbuff(1);
				}
			}
		}
		return;
	case 1:
		if(LCR.b.DLAB)
		{ //DLL
			DLM = data;
		}
		else
		{
			IER.v = data;
			update_irq();
		}
		return;
	case 2:
		FCR.v = data;
		FCR.b.RXFIFORST = 0; //TODO: fifo cleanup?
		FCR.b.TXFIFORST = 0; //TODO: fifo cleanup?
		return;
	case 3:
		LCR.v = data;
		return;
	case 4:
		MCR.v = data;
		update_irq();
		if(MCR.b.LOOP)
		{
			update_control_lines(MCR.b.RTS, MCR.b.DTR,true);
		}
		return;
	case 5:
		return;
	case 6:
		return;
	case 7:
		SCR = data;
		return;
	default:
		return;
	}
}

void UART_16550::poll()
{

}

void UART_16550::update_irq()
{
	if(!MCR.b.OUT2)
	{
		IRQ_Set(irqh, false);
	}
	else if(RLS_IRQ && IER.b.ELSI)
	{
		IRQ_Set(irqh, true);
	}
	else if (RX.fifo_check() >= rx_bytes_trigger() && IER.b.ERBFI)
	{
		IRQ_Set(irqh, true);
	}
	else if (CTO_IRQ && IER.b.ERBFI)
	{
		IRQ_Set(irqh, true);
	}
	else if (THRE_IRQ && IER.b.ETBEI)
	{
		IRQ_Set(irqh, true);
	}
	else if (MODEM_IRQ && IER.b.EDSSI)
	{
		IRQ_Set(irqh, true);
	}
	else
	{
		IRQ_Set(irqh, false);
	}
}


void UART_16550::connect(uint16_t port, uint8_t irq)
{
	add_device({
					.start = port,
					.size = (uint32_t)8,
					.type = Device::Type::IO,
					.rdfn = &CWrap<uint32_t,UART_16550,&UART_16550::io_rdfn>,
					.wrfn = &CWrap<void,UART_16550,&UART_16550::io_wrfn>,
					.obj = (void*)this,
	});
	irqh = IRQ_Create_Handle(irq);
}
