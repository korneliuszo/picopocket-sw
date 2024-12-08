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
		"push ds" \
		"mov ds,cx" \
		"mov cx,bx" \
		"l1:" \
		"lodsb" \
		"out dx,al" \
		"loop l1" \
		"pop ds" \
		parm [cx si] [bx] [dx] \
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

extern const unsigned  __based( __segname( "_CODE" ) ) port_nog;
#pragma aux port_nog "port_no";


int __cdecl start(uint16_t irq, IRQ_DATA far * params);
int start(uint16_t irq, IRQ_DATA far * params)
{
	unsigned port_no = port_nog;
	uint8_t sync_counter = 0;
	uint8_t rettype = irq == 0x01 ? 0x01 : 0x02;
	unsigned codeplace = (params->ph2 - (unsigned)&irqentry)/3-1; // call near takes 3 bytes
	outp(port_no+1,0x01);
	outp(port_no,irq);
	outp(port_no,codeplace);
	for(uint8_t far * i=(uint8_t far *)params, far *e = i+sizeof(*params);i<e;i++)
		outp(port_no,*i);
	outp(port_no,rettype);
	outp(port_no,0xed); //TODO: full isolation
	outp(0x80,0x00); //DEBUG
	while(1)
	{
		uint8_t req;
		while((req = inph(port_no))==0x00) //NotYET
		{
			//powersave??
		}
		outp(0x80,req); //DEBUG
		switch(req)
		{
		case 0x03:
			outp(port_no+1,0x00); //TODO: full isolation
			return retcode_map[rettype];
		default:
			while(1); //break
			break;
		case 0x01:
		{
			STACKCODE_DATA_t regs;
			uint8_t code[8];
			for(uint8_t far * i=(uint8_t far *)&regs, far *e = i+sizeof(regs);i<e;i++)
				*i=inph(port_no);
			for(uint8_t far * i=(uint8_t far *)code, far *e = i+8;i<e;i++)
				*i=inph(port_no);
			stackcode(&regs,code);
			for(uint8_t far * i=(uint8_t far*)&regs, far *e = i+sizeof(regs);i<e;i++)
				outp(port_no,*i);
			continue;
		 }
		case 0x02:
			for(uint8_t far * i=(uint8_t far *)params, far *e = i+sizeof(*params);i<e;i++)
				*i=inph(port_no);
			rettype = inph(port_no);
			continue;
		case 0x08:
		 {
			unsigned irq = inp(port_no);
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
			outp(port_no,old[0]);
			outp(port_no,old[0]>>8);
			outp(port_no,old[1]);
			outp(port_no,old[1]>>8);
			continue;
		 }
		case 0x04:
		 {
			unsigned port = inp_bew(port_no);
			unsigned val = inph(port_no);
			outp(port,val);
			continue;
		 }
		case 0x05:
		 {
			unsigned port = inp_bew(port_no);
			unsigned val = inph(port);
			outp(port_no,val);
			continue;
		 }
		case 0x06:
		 {
			__segment seg = inp_bew(port_no);
			uint8_t __based( seg ) * addr = (uint8_t __based( seg ) *)inp_bew(port_no);
			unsigned len = inp_bew(port_no);
			putmem(addr,len,port_no);
			continue;
		}
		case 0x07:
		 {
			__segment seg = inp_bew(port_no);
			uint8_t __based( seg ) * addr = (uint8_t __based( seg ) *)inp_bew(port_no);
			unsigned len = inp_bew(port_no);
			getmem(addr,len,port_no);
			continue;
		}
		case 0x09:
		 {
			 outp(port_no,sync_counter++);
			 continue;
		 }
	 }
	}
}

