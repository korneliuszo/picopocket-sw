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
		"rep insb" \
		parm [es di] [cx] [dx] \
		modify exact [al cx di cx];

void getmem(uint8_t far * addr,unsigned len, unsigned port);
#pragma aux getmem = \
		"push ds" \
		"mov ds,cx" \
		"mov cx,bx" \
		"rep outsb" \
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

int select_next(
		uint8_t far LastID[8],
		int far *lastConflictZero,
		int far *lastDevice,
		uint16_t irq,
		unsigned codeplace,
		IRQ_DATA far * params,
		uint8_t rettype
		)
{
	unsigned port_no = port_nog;
	int i;
	int BitNumber = 0;
	int ConflictBitNumber = *lastConflictZero;
	if(*lastDevice)
		return 0;
	*lastConflictZero = -1;

	outp(port_no+1,0x01);
	outp(port_no,irq);
	outp(port_no,codeplace);
	getmem((uint8_t far *)params,sizeof(*params),port_no);
	outp(port_no,rettype);

	for(i=7;i>=0;i--)
	{
		uint8_t bit;
		for(bit=0x80;bit;bit>>=1)
		{
			int is_zero;
			int is_one;
			is_zero= inp(port_no) == 0xa5;
			is_one = inp(port_no) == 0x5a;

			if(!is_zero && !is_one)
			{
				outp(port_no+1,0x00);
				return 0;
			}

			if(is_zero && is_one)
			{
				if(ConflictBitNumber != BitNumber)
				{
					if(BitNumber<ConflictBitNumber)
					{
						is_one = LastID[i]&bit;
					}
					else
					{
						is_one = 0;
					}
				}
				else
				{
					is_one = 1;
				}
				if(!is_one)
					*lastConflictZero = BitNumber;
			}
			if(is_one)
			{
				LastID[i]|=bit;
				outp(port_no,0x5a);
			}
			else
			{
				LastID[i]&=~bit;
				outp(port_no,0xa5);
			}
			BitNumber++;
		}
	}
	if(*lastConflictZero == -1)
		*lastDevice = 1;
	return 1;
}

int __cdecl start(uint16_t irq, IRQ_DATA far * params);
int start(uint16_t irq, IRQ_DATA far * params)
{
	uint8_t LastID[8];
	int lastConflictZero = -1;
	int lastDevice = 0;
	unsigned port_no = port_nog;
	uint8_t sync_counter = 0;
	uint8_t rettype = irq == 0x00 ? 0x02 : 0x7F;
	unsigned codeplace = (params->ph2 - (unsigned)&irqentry)/3-1; // call near takes 3 bytes
	if(!select_next(LastID,&lastConflictZero,&lastDevice,
			irq,
			codeplace,
			params,
			rettype))
	{
		outp(0x80,0xED); //DEBUG
		while(1);
	}
	next_board:
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
			outp(port_no+1,0x00);
			if(select_next(LastID,&lastConflictZero,&lastDevice,
					irq,
					codeplace,
					params,
					rettype))
				goto next_board;
			if(rettype == 0x7f)
				while(1); //break
			return retcode_map[rettype&0x03];
		default:
			while(1); //break
			break;
		case 0x01:
		{
			STACKCODE_DATA_t regs;
			uint8_t code[8];
			putmem((uint8_t far *)&regs,sizeof(regs),port_no);
			putmem(&code,sizeof(code),port_no);
			stackcode(&regs,code);
			getmem((uint8_t far *)&regs,sizeof(regs),port_no);

			continue;
		 }
		case 0x02:
			putmem((uint8_t far *)params,sizeof(*params),port_no);
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
		case 0x0A:
			outp(port_no,irq);
			outp(port_no,codeplace);
			for(uint8_t far * i=(uint8_t far *)params, far *e = i+sizeof(*params);i<e;i++)
				outp(port_no,*i);
			outp(port_no,rettype);
			continue;
	 }
	}
}

