#pragma once

#include <stdint.h>

namespace IoIface {
typedef uint32_t Handler_return_t;

struct Handler;
struct OHandler {
	virtual void wr_byte(uint8_t byte) =0;
	virtual Handler_return_t rd_byte() =0;
	virtual const Handler & ec() = 0; //invalid
	int stack_of_unrecognized = 0;
	OHandler * prev;
};

struct
Handler{
	const uint8_t ec;
	OHandler * (*acquire_object)(uint8_t pipe);
	void (*release_object)(OHandler *);
};

#define Handler_type_section [[gnu::section("ioiface_handlers"),gnu::used]] const

Handler_return_t response_byte(uint8_t byte);
Handler_return_t response_tristate();

void ioiface_install();

struct Arbitration {
	enum class RET {
		CONTINUE,
		WON,
		LOST,
	};
	bool rd_bit;
	uint8_t curr_byte;
	int bit_in_byte;
	int byte_in_uid;
	uint8_t uid[8];
	void reset()
	{
		rd_bit = false;
		bit_in_byte = 7;
		byte_in_uid = 7;
	}
	Handler_return_t rd_byte()
	{
		Handler_return_t ret;
		if(((uid[byte_in_uid] >> bit_in_byte)&0x01)^rd_bit)
			ret = response_tristate();
		else
			ret = response_byte(rd_bit? 0x5a:0xa5);
		rd_bit=!rd_bit;
		return ret;
	}
	RET wr_byte(uint8_t byte)
	{
		rd_bit=false;
		if (byte == 0xed)
			return RET::WON;
		if ((uid[byte_in_uid] >> bit_in_byte)&0x01)
		{
			if (byte != 0x5a)
				return RET::LOST;
		}
		else
		{
			if (byte != 0xa5)
				return RET::LOST;
		}
		if(!bit_in_byte)
		{
			bit_in_byte=7;
			if(!byte_in_uid)
				byte_in_uid-=1;
			else
				return RET::WON;
		}
		return RET::CONTINUE;
	}

};

};
