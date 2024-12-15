#include "config_iface.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

_WCIRTLINK extern unsigned inp(unsigned port);
_WCIRTLINK extern unsigned outp(unsigned port, unsigned value);
#pragma intrinsic(inp,outp)

unsigned port_no = 0xFA;

uint8_t LastID[8];

int lastConflictZero;
int lastDevice;

void ResetSelect()
{
	memset(LastID,0x00,sizeof(LastID));
	lastConflictZero = 0;
	lastDevice = 0;
}

void Select(uint8_t ID[8])
{
	int i;
	outp(port_no+1,0x02);
	for(i=7;i>=0;i--)
	{
		uint8_t bit;
		for(bit=0x80;bit;bit>>=1)
		{
			if(ID[i]&bit)
				outp(port_no,0x5a);
			else
				outp(port_no,0xa5);
		}
	}
}

int SelectNext()
{
	int i;
	int BitNumber = 0;
	int ConflictBitNumber = lastConflictZero;
	if(lastDevice)
		return 0;
	lastConflictZero = 0;
	outp(port_no+1,0x02);
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
				return 0;

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
					if(!is_one)
						lastConflictZero = BitNumber;
				}
				else
				{
					is_one = 1;
				}
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
	if(lastConflictZero == 0)
		lastDevice = 1;
	return 1;
}

void Release()
{
	outp(port_no+1,0x00);
}

void SendCmd(uint8_t cmd)
{
	outp(port_no,cmd);
}

void SendCommandUID(uint16_t uid)
{
	outp(port_no,uid);
	outp(port_no,uid>>8);
}

char* RecvData(char* str,uint16_t len)
{
	unsigned i;
	for(i=0;i<len;i++ )
	{
		str[i] = inp(port_no);
	}
	return str;
}

char* RecvString()
{
	uint16_t len;
	char * str;
	len = RecvU16();
	str = (char*)malloc(len+1);
	RecvData(str,len);
	str[len]=0;
	return str;
}

int SendString(char * str)
{
	outp(port_no,*str);
	while(*str)
		outp(port_no,*++str);
}

uint16_t RecvU16()
{
	uint16_t len;
	len = inp(port_no) & 0xFF;
	len |= inp(port_no)<<8;
	return len;
}
