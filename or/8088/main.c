#include <i86.h>
#include <stdint.h>
#include <string.h>

typedef struct {
	uint16_t es;
	uint16_t ds;
	uint16_t di;
	uint16_t si;
	uint16_t bp;
	uint16_t bx;
	uint16_t dx;
	uint16_t cx;
	uint16_t ax;
	uint16_t f; //use for stackcode base
	uint16_t ph1;
	uint16_t ph2;
	uint16_t ip;
	uint16_t cs;
	uint16_t rf; //only in irq
} IRQ_DATA;

_WCIRTLINK extern unsigned inp(unsigned port);
_WCIRTLINK extern unsigned outp(unsigned port, unsigned value);
#pragma intrinsic(inp,outp)

typedef struct {
	uint16_t es;
	uint16_t ds;
	uint16_t di;
	uint16_t si;
	uint16_t bx;
	uint16_t dx;
	uint16_t cx;
	uint16_t ax;
	uint16_t f;
} STACKCODE_DATA_t;

uint16_t int_query(void);
#pragma aux int_query =     \
    "pushf"                 \
    "pop    ax"             \
    value [ax] modify exact [ax] nomemory;

unsigned inp_bew(unsigned port);
#pragma aux inp_bew = \
		"in al,dx" \
		"mov ah,al" \
		"in al,dx" \
		parm [dx] \
		value [ax] modify exact [ax] nomemory;

uint8_t inph(unsigned port);
#pragma aux inph = \
		"in al,dx" \
		parm [dx] \
		value [al] modify exact [al] nomemory;

void putmem(uint8_t far * addr,unsigned len, unsigned port);
#pragma aux putmem = \
		"l1:" \
		"in al,dx" \
		"stosb" \
		"loop l1" \
		parm [es di] [cx] [dx] \
		modify exact [al cx di cx];

void getmem(uint8_t far * addr,unsigned len, unsigned port);
#pragma aux getmem = \
		"l1:" \
		"lodsb" \
		"out dx,al" \
		"loop l1" \
		parm [es si] [cx] [dx] \
		modify exact [al cx si cx];


uint16_t axconcat(uint8_t h, uint8_t l);
#pragma aux axconcat = \
	parm [ah] [al] \
	value [ax]

void stackcode(STACKCODE_DATA_t __far * params,uint8_t __far * code);
#pragma aux stackcode near \
	parm [bx cx] [ax dx] \
	modify exact [ax bx cx dx si di es];

const int retcode_map[] = { 1, 0, -1};
extern const uint8_t  __based( __segname( "_CODE" ) ) irqentry;
#pragma aux irqentry "irqentry";

int __cdecl start(uint16_t irq, IRQ_DATA far * params);
int start(uint16_t irq, IRQ_DATA far * params)
{
	uint8_t sync_counter = 0;
	uint8_t rettype = irq == 0x01 ? 0x01 : 0x02;
	unsigned codeplace = (params->ph2 - (unsigned)&irqentry)/3-1; // call near takes 3 bytes
	outp(0x89,0x01);
	outp(0x88,irq);
	outp(0x88,codeplace);
	for(uint8_t far * i=(uint8_t far *)params, far *e = i+sizeof(*params);i<e;i++)
		outp(0x88,*i);
	outp(0x88,rettype);
	outp(0x88,0xed); //TODO: full isolation
	outp(0x80,0x00); //DEBUG
	while(1)
	{
		uint8_t req;
		while((req = inph(0x88))==0x00) //NotYET
		{
			//powersave??
		}
		outp(0x80,req); //DEBUG
		switch(req)
		{
		case 0x03:
			outp(0x89,0x00); //TODO: full isolation
			return retcode_map[rettype];
		default:
			while(1); //break
			break;
		case 0x01:
		{
			STACKCODE_DATA_t regs;
			uint8_t code[8];
			for(uint8_t far * i=(uint8_t far *)&regs, far *e = i+sizeof(regs);i<e;i++)
				*i=inph(0x88);
			for(uint8_t far * i=(uint8_t far *)code, far *e = i+8;i<e;i++)
				*i=inph(0x88);
			stackcode(&regs,code);
			for(uint8_t far * i=(uint8_t far*)&regs, far *e = i+sizeof(regs);i<e;i++)
				outp(0x88,*i);
			continue;
		 }
		case 0x02:
			for(uint8_t far * i=(uint8_t far *)params, far *e = i+sizeof(*params);i<e;i++)
				*i=inph(0x88);
			rettype = inph(0x88);
			continue;
		case 0x08:
		 {
			unsigned irq = inp(0x88);
			unsigned far * IVT=0x0000:>((unsigned*)(irq*4));
			unsigned old[2];
			{ //ISR
				old[0] = IVT[0];
				old[1] = IVT[1];
				IVT[0] = FP_OFF(&irqentry+(3*irq));
				IVT[1] = FP_SEG(&irqentry);
			}
			if(old[0] == IVT[0] && old[1] == IVT[1])
				old[1] = old[0] = 0;
			outp(0x88,old[0]);
			outp(0x88,old[0]>>8);
			outp(0x88,old[1]);
			outp(0x88,old[1]>>8);
			continue;
		 }
		case 0x04:
		 {
			unsigned port = inp_bew(0x88);
			unsigned val = inph(0x88);
			outp(port,val);
			continue;
		 }
		case 0x05:
		 {
			unsigned port = inp_bew(0x88);
			unsigned val = inph(port);
			outp(0x88,val);
			continue;
		 }
		case 0x06:
		 {
			__segment seg = inp_bew(0x88);
			uint8_t __based( seg ) * addr = (uint8_t __based( seg ) *)inp_bew(0x88);
			unsigned len = inp_bew(0x88);
			putmem(addr,len,0x88);
			continue;
		}
		case 0x07:
		 {
			__segment seg = inp_bew(0x88);
			uint8_t __based( seg ) * addr = (uint8_t __based( seg ) *)inp_bew(0x88);
			unsigned len = inp_bew(0x88);
			getmem(addr,len,0x88);
			continue;
		}
		case 0x09:
		 {
			 outp(0x88,sync_counter++);
			 continue;
		 }
	 }
	}
}

