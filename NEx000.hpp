#pragma once

#include<stdint.h>

#include <isa_worker.hpp>
#include "pico/sync.h"
#include "hardware/sync.h"

namespace {
template
<typename R, class C, auto F, typename ... Args>
static R CWrap(void * v_obj, Args ... args)
{
	C * that = reinterpret_cast<C*>(v_obj);
	return (that->*F)(args...);
}
}

template<class RAM>
class DP8390{
private:

	static constexpr const size_t rd_reg[4][16] = {
			{
					[0x00] = offsetof(DP8390,cr_reg),
					[0x01] = offsetof(DP8390,clda_reg.b.lo),
					[0x02] = offsetof(DP8390,clda_reg.b.hi),
					[0x03] = offsetof(DP8390,bnry_reg),
					[0x04] = offsetof(DP8390,tsr_reg),
					[0x05] = offsetof(DP8390,ncr_reg),
					[0x06] = offsetof(DP8390,val_bad_reg),
					[0x07] = offsetof(DP8390,isr_reg),
					[0x08] = offsetof(DP8390,crda_reg.b.lo),
					[0x09] = offsetof(DP8390,crda_reg.b.hi),
					[0x0a] = offsetof(DP8390,val_bad_reg),
					[0x0b] = offsetof(DP8390,val_bad_reg),
					[0x0c] = offsetof(DP8390,rsr_reg),
					[0x0d] = offsetof(DP8390,cntr0_reg),
					[0x0e] = offsetof(DP8390,cntr1_reg),
					[0x0f] = offsetof(DP8390,cntr2_reg),
			},
			{
					[0x00] = offsetof(DP8390,cr_reg),
					[0x01] = offsetof(DP8390,par_reg)+0,
					[0x02] = offsetof(DP8390,par_reg)+1,
					[0x03] = offsetof(DP8390,par_reg)+2,
					[0x04] = offsetof(DP8390,par_reg)+3,
					[0x05] = offsetof(DP8390,par_reg)+4,
					[0x06] = offsetof(DP8390,par_reg)+5,
					[0x07] = offsetof(DP8390,curr_reg),
					[0x08] = offsetof(DP8390,mar_reg)+0,
					[0x09] = offsetof(DP8390,mar_reg)+1,
					[0x0a] = offsetof(DP8390,mar_reg)+2,
					[0x0b] = offsetof(DP8390,mar_reg)+3,
					[0x0c] = offsetof(DP8390,mar_reg)+4,
					[0x0d] = offsetof(DP8390,mar_reg)+5,
					[0x0e] = offsetof(DP8390,mar_reg)+6,
					[0x0f] = offsetof(DP8390,mar_reg)+7,
			},
			{
					[0x00] = offsetof(DP8390,cr_reg),
					[0x01] = offsetof(DP8390,pstart_reg),
					[0x02] = offsetof(DP8390,pstop_reg),
					[0x03] = offsetof(DP8390,val_bad_reg),
					[0x04] = offsetof(DP8390,tpsr_reg),
					[0x05] = offsetof(DP8390,val_bad_reg),
					[0x06] = offsetof(DP8390,val_bad_reg),
					[0x07] = offsetof(DP8390,val_bad_reg),
					[0x08] = offsetof(DP8390,val_bad_reg),
					[0x09] = offsetof(DP8390,val_bad_reg),
					[0x0a] = offsetof(DP8390,val_bad_reg),
					[0x0b] = offsetof(DP8390,val_bad_reg),
					[0x0c] = offsetof(DP8390,rcr_reg),
					[0x0d] = offsetof(DP8390,tcr_reg),
					[0x0e] = offsetof(DP8390,dcr_reg),
					[0x0f] = offsetof(DP8390,imr_reg),
			},
			{
					[0x00] = offsetof(DP8390,cr_reg),
					[0x01] = offsetof(DP8390,val_bad_reg),
					[0x02] = offsetof(DP8390,val_bad_reg),
					[0x03] = offsetof(DP8390,val_bad_reg),
					[0x04] = offsetof(DP8390,val_bad_reg),
					[0x05] = offsetof(DP8390,val_bad_reg),
					[0x06] = offsetof(DP8390,val_bad_reg),
					[0x07] = offsetof(DP8390,val_bad_reg),
					[0x08] = offsetof(DP8390,val_bad_reg),
					[0x09] = offsetof(DP8390,val_bad_reg),
					[0x0a] = offsetof(DP8390,val_bad_reg),
					[0x0b] = offsetof(DP8390,val_bad_reg),
					[0x0c] = offsetof(DP8390,val_bad_reg),
					[0x0d] = offsetof(DP8390,val_bad_reg),
					[0x0e] = offsetof(DP8390,val_bad_reg),
					[0x0f] = offsetof(DP8390,val_bad_reg),
			},
	};

	volatile RAM local_ram;
	union uint16_t_LE
	{
		uint16_t w;
		struct {
			uint8_t lo;
			uint8_t hi;
		} b;
	};

	volatile uint16_t_LE clda_reg;
	volatile uint16_t_LE crda_reg;
	volatile uint16_t_LE tbcr_reg;
	volatile uint16_t_LE rsar_reg;
	volatile uint16_t_LE rbcr_reg;
	volatile const uint8_t val_bad_reg = 0xcc;
	volatile uint8_t cr_reg;
	volatile uint8_t imr_reg;
	volatile uint8_t bnry_reg;
	volatile uint8_t tsr_reg;
	volatile uint8_t ncr_reg;
	volatile uint8_t isr_reg;
	volatile uint8_t rsr_reg;
	volatile uint8_t cntr0_reg;
	volatile uint8_t cntr1_reg;
	volatile uint8_t cntr2_reg;
	volatile uint8_t par_reg[6];
	volatile uint8_t curr_reg;
	volatile uint8_t mar_reg[8];
	volatile uint8_t pstart_reg;
	volatile uint8_t pstop_reg;
	volatile uint8_t tpsr_reg;
	volatile uint8_t rcr_reg;
	volatile uint8_t tcr_reg;
	volatile uint8_t dcr_reg; //LAS
	volatile bool phy_bit;
	volatile bool prx_bit;

	volatile uint8_t mac[32];

	void Recalculate_IRQ()
	{
		IRQ_Set(irqh,imr_reg&isr_reg&0x7f);
	}
	ssize_t dma_op_cnt;
	void DMA_Complete(){
		dma_op_cnt = -1;
	};
	void DMA_Read(){
		crda_reg.w = rsar_reg.w;
		dma_op_cnt = rbcr_reg.w;
		if(!dma_op_cnt) // linux irq detection
		{
			uint32_t save = spin_lock_blocking(spinlock);
			isr_reg |= 0x40; //RDC
			spin_unlock(spinlock,save);
		}
	};
	void DMA_Write(){
		crda_reg.w = rsar_reg.w;
		dma_op_cnt = rbcr_reg.w;
	};
	volatile size_t tx_packet = 0;
	volatile size_t tx_packet_len = 0;
	void Send_Packet(){
		tx_packet_len = tbcr_reg.w;
		tx_packet = tpsr_reg*256;
	};
	uint32_t rdfn(uint32_t addr)
	{


		if(addr<0x10)
		{
			const size_t page = (cr_reg>>6)&0x3;
			//if(page == 0 && addr == 0x06) //FIFO
			//	return 0xff; //todo
			const size_t off = rd_reg[page][addr];
			if(off == offsetof(DP8390,val_bad_reg))
				return ISA_FAKE_READ; //don't drive unknown registers
			return reinterpret_cast<const uint8_t*>(this)[off];
		}
		if(addr<0x18)
		{
			if(((cr_reg>>3)&0x7) == 0x1)
			{
				uint16_t i = crda_reg.w - (addr&0x01);
				uint16_t step = 0;
				if(!(addr&0x01))
					step = dcr_reg & 0x01 ? 2 : 1;
				crda_reg.w+=step;
				if((dma_op_cnt-=step)<=0 && (!(dcr_reg & 0x01) || (addr&0x01)))
				{
					uint32_t save = spin_lock_blocking(spinlock);
					cr_reg = 0x20 | (cr_reg&~0x38);
					isr_reg |= 0x40; //RDC
					spin_unlock(spinlock,save);
					Recalculate_IRQ();
				}
				if(i >= local_ram.buff_start && i < local_ram.buff_end)
					return local_ram.buff[i - local_ram.buff_start];
				else if (i<32)
					return mac[i];
				return 0xff;
			}
		}
		return ISA_FAKE_READ;
	}
	void wrfn(uint32_t addr, uint8_t data)
	{
		if(addr == 0) //CR
		{
			uint32_t save = spin_lock_blocking(spinlock);
			cr_reg = (cr_reg&0x04) | data; //TXP
			spin_unlock(spinlock,save);
			if(data&0x20) // Abort/Complete remote DMA
				DMA_Complete();
			if ((data&0x04) && !tx_packet) //TXP
				return Send_Packet();
			else
				switch((data>>3)&0x7) //DMA_OP
				{
				case 1:
					return DMA_Read();
				case 2:
					return DMA_Write();
				case 3:;
					return;
				case 0: //not allowed
				default:
					break;
				};
			return;
		}
		if (addr >= 0x18)
			return Reset();
		if (addr >=0x10)
		{
			if(((cr_reg>>3)&0x7) == 0x2)
			{
				uint16_t i = crda_reg.w - (addr&0x01);
				uint16_t step = 0;
				if(!(addr&0x01))
					step = (dcr_reg & 0x01) ? 2 : 1;
				crda_reg.w+=step;
				if (
					i >= local_ram.buff_start &&
					i < local_ram.buff_end)
						local_ram.buff[i - local_ram.buff_start] = data;

				if((dma_op_cnt-=step)<=0 && (!(dcr_reg & 0x01) || (addr&0x01)))
				{
					uint32_t save = spin_lock_blocking(spinlock);
					cr_reg = 0x20 | (cr_reg&~0x38);
					isr_reg |= 0x40; //RDC
					spin_unlock(spinlock,save);
					Recalculate_IRQ();
				}
			}
			return;
		}
		else
		{
			switch(cr_reg>>6) //PS0,1
			{
			case 0: //page 0
				switch(addr)
				{
				case 0x01: //PSTART
					pstart_reg = data;
					return;
				case 0x02: //PSTOP
					pstop_reg = data;
					return;
				case 0x03: //BNRY
					bnry_reg = data;
					return;
				case 0x04: //TPSR
					tpsr_reg = data;
					return;
				case 0x05:
					tbcr_reg.b.lo = data;
					return;
				case 0x06:
					tbcr_reg.b.hi = data;
					return;
				case 0x07: //ISR
				{	uint32_t save = spin_lock_blocking(spinlock);
					isr_reg &=~data;
					spin_unlock(spinlock,save);
					return Recalculate_IRQ();
				}
				case 0x08:
					rsar_reg.b.lo = data;
					return;
				case 0x09:
					rsar_reg.b.hi = data;
					return;
				case 0x0a:
					rbcr_reg.b.lo = data;
					return;
				case 0x0b:
					rbcr_reg.b.hi = data;
					return;
				case 0x0c:
					rcr_reg = data | 0xC0;
					rsr_recalc();
					return;
				case 0x0d:
					tcr_reg = data | 0xE0;
					return;
				case 0x0e:
					dcr_reg = data;
					return;
				case 0x0f: //IMR
					imr_reg = data;
					return Recalculate_IRQ();
				default:
					return;
				}
			case 1: //page 1
				if(addr<0x07)
					par_reg[addr-0x1] = data;
				else if (addr == 0x07)
					curr_reg = data;
				else
					mar_reg[addr-0x08] = data;
				return;
			case 2: //page 2
				return;
			case 3: //page 3
				return;
			};
			return;
		}
	}
	uint8_t irqh;
	void rsr_recalc()
	{
		rsr_reg = ((rcr_reg & 0x20) ? 0x40 : 0 ) //copy MON to DIS
				| (phy_bit ? 0x20 : 0)
				| (prx_bit ? 0x01 : 0x10);
	}
public:
	void Reset()
	{
		cr_reg = 0x21;
		isr_reg = 0x80;
		imr_reg = 0;
		tcr_reg = 0;
		dcr_reg = 0x04; //LAS
		tx_packet = 0;
		rcr_reg = 0xC0;
		rsr_recalc();
		tcr_reg = 0xE0;
		Recalculate_IRQ();
		DMA_Complete();
	}
	typedef void (*TX)(uint8_t* buff, size_t len);
private:
	TX tx_handler;
	spin_lock_t* spinlock;
public:
	DP8390() // @suppress("Class members should be properly initialized")
	{
		spinlock = spin_lock_init(spin_lock_claim_unused(true));
		Reset();
	}
	void Connect_ISA(uint16_t base_port,uint8_t _irqh, TX _tx_handler, const uint8_t _mac[6])
	{
		irqh = _irqh;
		tx_handler = _tx_handler;

		mac[0] = _mac[0];
		mac[1] = _mac[0];
		mac[2] = _mac[1];
		mac[3] = _mac[1];
		mac[4] = _mac[2];
		mac[5] = _mac[2];
		mac[6] = _mac[3];
		mac[7] = _mac[3];
		mac[8] = _mac[4];
		mac[9] = _mac[4];
		mac[10] = _mac[5];
		mac[11] = _mac[5];
		for(int i=12;i<32;i++)
			mac[i] = 0x57;

		Reset();
		add_device({
			base_port,
			0x20,
			Device::Type::IO,
			&CWrap<uint32_t,DP8390,&DP8390::rdfn>,
			&CWrap<void,DP8390,&DP8390::wrfn>,
			this
		});
	}
	void update_polled_state()
	{
		if(tx_packet && !(cr_reg &0x01))
		{
			if(tx_packet>=local_ram.buff_start && tx_packet+tx_packet_len<local_ram.buff_end)
				tx_handler((uint8_t*)&local_ram.buff[tx_packet-local_ram.buff_start],tx_packet_len);
			tx_packet=0;
			tsr_reg |= 0x01;//PTX
			uint32_t save = spin_lock_blocking(spinlock);
			cr_reg &= ~0x04;//TXP
			isr_reg |=0x02;//PTX
			spin_unlock(spinlock,save);
			Recalculate_IRQ();
		}
	}

	static bool is_multicast(const uint8_t *pkt,size_t len)
	{
		return pkt[0]&0x01;
	}

	static bool is_broadcast(const uint8_t *pkt,size_t len)
	{
		int i=0;
		do
			if(pkt[i]!=0xff)
				break;
		while(++i<6);
		return i == 6;;
	}

	void rx_packet(const uint8_t *pkt,size_t len)
	{
		if(cr_reg & 0x01) //STP
			return;
		if(tcr_reg & 0x06) //loopback
			return;
		if((phy_bit = is_multicast(pkt,len)))
		{	if(is_broadcast(pkt,len))
			{	if(!(rcr_reg & 0x14)) //PRO || AB
					return;
			} else
			{	if(!(rcr_reg & 0x18)) //PRO || AM
					return;
				//multicast filtering here
				//to make them work - needs enabling all multicasts in wifi driver
			}
		} else
		{
			if(memcmp((void*)par_reg,pkt,6))
				if(!(rcr_reg & 0x10)) //PRO
					return;
		}
		if (len < 64 && !(rcr_reg & 0x02)) //AR
			len=64; // pad to min eth packet size

		if(rcr_reg & 0x20) //MON
		{	prx_bit = false;
			rsr_recalc();
			return;
		}

		uint page_len=(len+4+255)>>8;

		int left_bnry = (int)bnry_reg - (int)curr_reg;
		if((left_bnry <= 0))
			left_bnry += (pstop_reg - pstart_reg);

		if(left_bnry - 1 < page_len)
		{	prx_bit = false;
			rsr_recalc();
			uint32_t save = spin_lock_blocking(spinlock);
			isr_reg |= 0x10; //OVW
			spin_unlock(spinlock,save);
			Recalculate_IRQ();
			return;
		}
		prx_bit = true;
		rsr_recalc();

		uint next_page = curr_reg + page_len;

		bool looped = (next_page >= pstop_reg);
		if(looped)
			next_page -= pstop_reg - pstart_reg;

		if(((curr_reg<<8) >= local_ram.buff_start)
		&& ((curr_reg<<8) < local_ram.buff_end)
		&& ((next_page<<8) >= local_ram.buff_start)
		&& ((next_page<<8) < local_ram.buff_end)
		&& ((pstart_reg<<8)>= local_ram.buff_start)
		&& ((pstop_reg<<8) <= local_ram.buff_end)
		)
		{	volatile uint8_t * pkt_mem = &local_ram.buff[(curr_reg<<8)-local_ram.buff_start];
			pkt_mem[0] = rsr_reg;
			pkt_mem[1] = next_page;
			pkt_mem[2] = (len+4);
			pkt_mem[3] = (len+4)>>8;

			if(looped)
			{
				uint looplen = ((pstop_reg-curr_reg)<<8)-4;
				if(len > looplen)
				{	memcpy((void*)&local_ram.buff[(pstart_reg<<8)-local_ram.buff_start],&pkt[looplen],len-looplen);
					len = looplen;
				}
			}
			memcpy((void*)&pkt_mem[4],pkt,len);
			__dmb();
		}

		curr_reg = next_page;

		uint32_t save = spin_lock_blocking(spinlock);
		isr_reg |= 0x01; //PRX
		spin_unlock(spinlock,save);
		Recalculate_IRQ();
	}
};

class NE1000_RAM
{
public:
	constexpr static const size_t buff_len = 8*1024;
	constexpr static const size_t buff_start = 8*1024;
	constexpr static const size_t buff_end = buff_len + buff_start;
	uint8_t buff[buff_len];
};

class NE2000_RAM
{
public:
	constexpr static const size_t buff_len = 16*1024;
	constexpr static const size_t buff_start = 16*1024;
	constexpr static const size_t buff_end = buff_len + buff_start;
	uint8_t buff[buff_len];
};

using NE1000 = DP8390<NE1000_RAM>;
using NE2000 = DP8390<NE2000_RAM>;
