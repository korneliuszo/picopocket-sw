/*
 * SPDX-FileCopyrightText: Korneliusz Osmenda <korneliuszo@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */
#include "tusb.h"
#include "pico/bootrom.h"
#include "device/usbd_pvt.h"
#include "hardware/gpio.h"
#include "config.hpp"

class Config_Itf {
	static inline uint8_t itf_num;
	static void init(void)
	{

	}
	static void reset(uint8_t rhport)
	{
		itf_num = 0;
	}
	static uint16_t open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len)
	{
		TU_VERIFY(TUSB_CLASS_VENDOR_SPECIFIC == itf_desc->bInterfaceClass &&
				'C' == itf_desc->bInterfaceSubClass &&
				0 == itf_desc->bInterfaceProtocol, 0);

		uint16_t drv_len = sizeof(tusb_desc_interface_t);
		TU_VERIFY(max_len >= drv_len, 0);

		itf_num = itf_desc->bInterfaceNumber;

		return drv_len;
	}

	static inline char buff[Config::USB_Access::MAX_MAX_STRLEN];

	static bool control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
	{
		TU_VERIFY(request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE);
		TU_VERIFY(request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS);

		if (stage == CONTROL_STAGE_SETUP)
		{
			switch(request->bRequest)
			{
			case 0:
				reset_usb_boot(0, 1);
				break;
				// does not return, otherwise we'd return true
			/*
			case 1: //get_uids
			{
				TU_VERIFY(request->bmRequestType_bit.direction == TUSB_DIR_IN);
				return tud_control_xfer(rhport, request, (uint8_t*)Config::Flash_Storage::uids,sizeof(Config::Flash_Storage::uids));

			}
			*/
			case 2: //get_name
			{
				TU_VERIFY(request->bmRequestType_bit.direction == TUSB_DIR_IN);
				uint16_t uid = request->wValue;
				auto celem = Config::USB_Access::uid(uid);
				if(!celem) return false;
				return tud_control_xfer(rhport, request, (uint8_t*)celem->name,strlen(celem->name));
			}
			case 3: //get_help
			{
				TU_VERIFY(request->bmRequestType_bit.direction == TUSB_DIR_IN);
				uint16_t uid = request->wValue;
				auto celem = Config::USB_Access::uid(uid);
				if(!celem) return false;
				return tud_control_xfer(rhport, request, (uint8_t*)celem->help,strlen(celem->help));
			}
			case 4: //get_type
			{
				TU_VERIFY(request->bmRequestType_bit.direction == TUSB_DIR_IN);
				uint16_t uid = request->wValue;
				auto celem = Config::USB_Access::uid(uid);
				if(!celem) return false;
				buff[0] = celem->to_flash;
				buff[1] = celem->coldboot_required;
				buff[2] = celem->is_directory;
				return tud_control_xfer(rhport, request, (uint8_t*)buff,3);
			}
			case 5: //get_val
			{
				TU_VERIFY(request->bmRequestType_bit.direction == TUSB_DIR_IN);
				uint16_t uid = request->wValue;
				auto celem = Config::USB_Access::uid(uid);
				if(!celem) return false;
				size_t len = celem->repr(celem->val,buff);
				return tud_control_xfer(rhport, request, (uint8_t*)buff,len);
			}
			case 6: //set_val
			{
				TU_VERIFY(request->bmRequestType_bit.direction == TUSB_DIR_OUT);
		        TU_VERIFY(request->wLength <= sizeof(buff));
		        return tud_control_xfer(rhport, request, buff, request->wLength);
			}
			case 7: //reset_val
			{
				TU_VERIFY(request->bmRequestType_bit.direction == TUSB_DIR_OUT);
				uint16_t uid = request->wValue;
				auto celem = Config::USB_Access::uid(uid);
				if(!celem) return false;
				celem->reset();
				return tud_control_status(rhport, request);
			}
			case 8: //flash_ops
			{
				TU_VERIFY(request->bmRequestType_bit.direction == TUSB_DIR_OUT);
				switch(request->wValue)
				{
				case 0:
					Config::Flash_Storage::load();
					return tud_control_status(rhport, request);
				case 1:
					Config::Flash_Storage::save();
					return tud_control_status(rhport, request);
				}
				return false;
			}
			default:
				return false;
			}
		} //setup
		else if (stage == CONTROL_STAGE_ACK) {
			switch(request->bRequest)
			{
			case 6: //set_val
			{
				TU_VERIFY(request->bmRequestType_bit.direction == TUSB_DIR_OUT);
				uint16_t uid = request->wValue;
				auto celem = Config::USB_Access::uid(uid);
				if(!celem) return false;
				celem->set_repr(celem->val,buff);
				return true;
			}
			}
			return true;
		}
		return true;
	}

	[[gnu::section(".tusb_driver_class"), gnu::used]]
	static inline usbd_class_driver_t const _config_driver =
	{
	#if CFG_TUSB_DEBUG >= 2
			.name = "MONITOR",
	#endif
			.init             = init,
			.reset            = reset,
			.open             = open,
			.control_xfer_cb  = control_xfer_cb,
			.xfer_cb          = NULL,
			.sof              = NULL
	};


};
