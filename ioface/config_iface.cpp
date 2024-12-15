#include "ioiface.hpp"
#include "config_iface.hpp"
#include "../config.hpp"
#ifndef PICOPOCKET_SIM
#include "pico/multicore.h"
#endif

void install_config_iface()
{
#ifndef PICOPOCKET_SIM
	multicore_lockout_victim_init();
#endif
}

struct ConfigHandler : public IoIface::OHandler {
	enum class STATE {
		NO_OP,
		ARBITRATION,
		READ_CMD,
		READ_STR,
		READ_STR2,
		READ_STR3,
		GET_UID,
		GET_UID2,
		SEND_U16,
		SEND_U161,
		GET_STR,
	};
	STATE state;
	IoIface::Arbitration arbiter;
	char buff[Config::IO_Access::MAX_MAX_STRLEN+1];
	const char * strptr;
	char * rstrptr;
	size_t strl;
	uint16_t uid;
	uint16_t u16val;
	uint8_t cmd;
	virtual void wr_byte(uint8_t byte)
	{
		switch (state){
		case STATE::NO_OP:
		default:
			return;
		case STATE::ARBITRATION:
		 {
			auto ret=arbiter.wr_byte(byte);
			switch(ret){
			case IoIface::Arbitration::RET::CONTINUE:
				return;
			case IoIface::Arbitration::RET::WON:
				state = STATE::READ_CMD;
				return;
			default:
			case IoIface::Arbitration::RET::LOST:
				state = STATE::NO_OP;
				return;
			}
			return; //pedantic
		 }
		case STATE::READ_CMD:
			switch(byte)
			{
			case 0x00:
				strl = Config::USB_SERIAL_NO::val.repr(buff)+1;
				strptr = buff;
				state = STATE::READ_STR;
				return;
			case 0x01:
				u16val = Config::IO_Access::FIELD_COUNT;
				state = STATE::SEND_U16;
				return;
			case 0x03:
			case 0x04:
			case 0x05:
			case 0x06:
				state = STATE::GET_UID;
				cmd = byte;
				return;
			case 0x07:
#ifndef PICOPOCKET_SIM
				multicore_lockout_start_blocking();
#endif
				Config::IO_Access::TSaved::save();
#ifndef PICOPOCKET_SIM
				multicore_lockout_end_blocking();
#endif
				state = STATE::READ_CMD;
				return;
			case 0x08:
				Config::IO_Access::TSaved::load();
				state = STATE::READ_CMD;
				return;
			default:
				state = STATE::NO_OP;
				return;
			}
		case STATE::GET_UID:
			uid = byte;
			state = STATE::GET_UID2;
			return;
		case STATE::GET_UID2:
		{
			uid |= byte<<8;
			auto celem = Config::IO_Access::uid(uid);
			if(!celem)
			{
				state = STATE::NO_OP;
				return;
			}
			switch(cmd)
			{
			case 0x03:
				strl = strlen(celem->name);
				strptr = celem->name;
				state = STATE::READ_STR;
				return;
			case 0x04:
				buff[0] = celem->to_flash;
				buff[1] = celem->coldboot_required;
				buff[2] = celem->is_directory;
				strl = 3;
				strptr = buff;
				state = STATE::READ_STR;
				return;
			case 0x05:
				strl = celem->repr(celem->val,buff);
				strptr = buff;
				state = STATE::READ_STR;
				return;
			case 0x06:
				rstrptr = buff;
				state = STATE::GET_STR;
				return;
			default:
				state = STATE::NO_OP;
				return;
			}
		}
		case STATE::GET_STR:
			*rstrptr++ = byte;
			if(!byte)
			{
				switch(cmd)
				{
				case 0x06:
				{
					auto celem = Config::IO_Access::uid(uid);
					//was checked before
					celem->set_repr(celem->val,buff);
					state = STATE::READ_CMD;
					return;
				}
				default:
					state = STATE::NO_OP;
					return;
				}
			}
			return;
		}
	}
	virtual IoIface::Handler_return_t rd_byte()
	{
		switch(state)
		{
		default:
			return IoIface::response_tristate();
		case STATE::ARBITRATION:
			return arbiter.rd_byte();
		case STATE::READ_STR:
			state = STATE::READ_STR2;
			return strl;
		case STATE::READ_STR2:
			state = STATE::READ_STR3;
			return strl>>8;
		case STATE::READ_STR3:
			if(!--strl)
				state = STATE::READ_CMD;
			return *strptr++;
		case STATE::SEND_U16:
			state = STATE::SEND_U161;
			return u16val;
		case STATE::SEND_U161:
			state = STATE::READ_CMD;
			return u16val>>8;
		}
	}
	virtual const IoIface::Handler & ec() { return IoIface::config_io_handler;};
	void reset()
	{
		state = STATE::ARBITRATION;
		arbiter.reset();
	}
};

static ConfigHandler handles[4];

static IoIface::OHandler * acquire_object(uint8_t pipe)
{
	if(pipe > 4)
		return nullptr;
	handles[pipe].reset();
	return &handles[pipe];
}
static void release_object(IoIface::OHandler * handle)
{
	reinterpret_cast<ConfigHandler*>(handle)->reset();
}

const IoIface::Handler IoIface::config_io_handler = {
		0x02,
		acquire_object,
		release_object
};
