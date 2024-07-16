#pragma once
#include "config_classes.hpp"

namespace Config {


enum class UID : uint16_t {
	ROOT_DIR,
	USB_DIR,
		USB_SERIAL_NO,
	BIOS_DIR,
	BIOS_SEGMENT,
	IO_PORT,
};

constexpr char USB_SERIAL_NO_DEFAULT[] = "NONAME";

class USB_SERIAL_NO : public Impl::CElem<USB_SERIAL_NO,UID::USB_SERIAL_NO,Impl::Str<USB_SERIAL_NO,31>,USB_SERIAL_NO_DEFAULT>{
public:
	static constexpr bool coldboot_required = false;
	static constexpr char help[] = "usb serial number";
	static bool validate(const char * newval)
	{
		return true;
	};
	static constexpr char name[] = "USB_SERIAL_NO";

};

class UsbDir : public Impl::Dir<UsbDir,UID::USB_DIR,USB_SERIAL_NO>{
public:
	static constexpr char help[] = "usb dir";
	static constexpr char name[] = "USB_DIR";
};

class BIOS_SEGMENT : public Impl::CElem<BIOS_SEGMENT,UID::BIOS_SEGMENT,Impl::HexInt<BIOS_SEGMENT,uint16_t>,0xc800>{
public:
	static constexpr bool coldboot_required = true;
	static constexpr char help[] = "Bios segment address";
	static bool validate(uint16_t newval)
	{
		if((newval > 0xc800) && (newval < 0xf000))
			if (newval%(2*1024/8)==0)
				return true;
		return false;
	};
	static constexpr char name[] = "BIOS_SEGMENT";
};

class IO_PORT : public Impl::CElem<IO_PORT,UID::IO_PORT,Impl::HexInt<IO_PORT,uint16_t>,0x88>{
public:
	static constexpr bool coldboot_required = true;
	static constexpr char help[] = "io port base";
	static bool validate(uint16_t)
	{
		return true;
	};
	static constexpr char name[] = "IO_PORT";
};

class BiosDir : public Impl::Dir<BiosDir,UID::BIOS_DIR,BIOS_SEGMENT>{
public:
	static constexpr char help[] = "bios dir";
	static constexpr char name[] = "bios Dir";
};

class RootDir : public Impl::Dir<RootDir,UID::ROOT_DIR,UsbDir,BiosDir,IO_PORT>{
public:
	static constexpr char help[] = "root_dir";
	static constexpr char name[] = "ROOT_DIR";
};

using Flash_Storage = Impl::FlashSaved<USB_SERIAL_NO,BIOS_SEGMENT,IO_PORT>;
using Config = Impl::BasicConfig<Flash_Storage,RootDir>;
using USB_Access = Config;
}
