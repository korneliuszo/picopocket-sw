#pragma once
#include "config_classes.hpp"

namespace Config {


enum class UID : uint16_t {
	ROOT_DIR,
	USB_DIR,
		USB_SERIAL_NO,
	WIFI_DIR,
		WIFI_SSID,
		WIFI_PW,
		WIFI_DHCP,
		WIFI_IP,
		WIFI_MASK,
		WIFI_GW,
		WIFI_DNS,
		WIFI_HOSTNAME,
		WIFI_CONNECTED,
	BIOS_DIR,
		BIOS_SEGMENT,
		MONITOR_AUTOBOOT,
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

constexpr char WIFI_SSID_DEFAULT[] = "Picopocket";

class WIFI_SSID : public Impl::CElem<WIFI_SSID,UID::WIFI_SSID,Impl::Str<WIFI_SSID,65>,WIFI_SSID_DEFAULT>{
public:
	static constexpr bool coldboot_required = false;
	static constexpr char help[] = "SSID of network";
	static bool validate(const char * newval)
	{
		return true;
	};
	static constexpr char name[] = "WIFI_SSID";

};

constexpr char WIFI_PW_DEFAULT[] = "";

class WIFI_PW : public Impl::CElem<WIFI_PW,UID::WIFI_PW,Impl::Str<WIFI_PW,65>,WIFI_PW_DEFAULT>{
public:
	static constexpr bool coldboot_required = false;
	static constexpr char help[] = "password of network";
	static bool validate(const char * newval)
	{
		return true;
	};
	static constexpr char name[] = "WIFI_PW";
};

class WIFI_CONNECTED : public Impl::CElem<WIFI_CONNECTED,UID::WIFI_CONNECTED,Impl::HexInt<WIFI_CONNECTED,uint8_t>,0x0>{
public:
	static constexpr bool coldboot_required = false;
	static constexpr char help[] = "wifi connection status";
	static bool validate(uint16_t v)
	{
		return v==0;
	};
	static constexpr char name[] = "WIFI_CONNECTED";
};

class WIFI_DHCP : public Impl::CElem<WIFI_DHCP,UID::WIFI_DHCP,Impl::HexInt<WIFI_DHCP,uint8_t>,0x1>{
public:
	static constexpr bool coldboot_required = true;
	static constexpr char help[] = "wifi use DHCP";
	static bool validate(uint16_t v)
	{
		return v==0 || v==1;
	};
	static constexpr char name[] = "WIFI_DHCP";
};


class WIFI_IP : public Impl::CElem<WIFI_IP,UID::WIFI_IP,Impl::IP<WIFI_IP>,0x0>{
public:
	static constexpr bool coldboot_required = false;
	static constexpr char help[] = "wifi IP";
	static bool validate(uint32_t v)
	{
		return true;
	};
	static constexpr char name[] = "WIFI_IP";
};

class WIFI_MASK : public Impl::CElem<WIFI_MASK,UID::WIFI_MASK,Impl::IP<WIFI_MASK>,0x0>{
public:
	static constexpr bool coldboot_required = false;
	static constexpr char help[] = "wifi netmask";
	static bool validate(uint32_t v)
	{
		return true;
	};
	static constexpr char name[] = "WIFI_MASK";
};

class WIFI_GW : public Impl::CElem<WIFI_GW,UID::WIFI_GW,Impl::IP<WIFI_GW>,0x0>{
public:
	static constexpr bool coldboot_required = false;
	static constexpr char help[] = "wifi gateway";
	static bool validate(uint32_t v)
	{
		return true;
	};
	static constexpr char name[] = "WIFI_GW";
};

class WIFI_DNS : public Impl::CElem<WIFI_DNS,UID::WIFI_DNS,Impl::IP<WIFI_DNS>,0x0>{
public:
	static constexpr bool coldboot_required = false;
	static constexpr char help[] = "wifi DNS server";
	static bool validate(uint32_t v)
	{
		return true;
	};
	static constexpr char name[] = "WIFI_DNS";
};

constexpr char WIFI_HOSTNAME_DEFAULT[] = "Picopocket";

class WIFI_HOSTNAME : public Impl::CElem<WIFI_HOSTNAME,UID::WIFI_HOSTNAME,Impl::Str<WIFI_HOSTNAME,33>,WIFI_HOSTNAME_DEFAULT>{
public:
	static constexpr bool coldboot_required = false;
	static constexpr char help[] = "hostname - check local dns";
	static bool validate(const char * newval)
	{
		return true;
	};
	static constexpr char name[] = "WIFI_HOSTNAME";

};

class WIFI_DIR : public Impl::Dir<WIFI_DIR,UID::WIFI_DIR,
	WIFI_SSID,
	WIFI_PW,
	WIFI_CONNECTED,
	WIFI_DHCP,
	WIFI_IP,
	WIFI_MASK,
	WIFI_GW,
	WIFI_DNS,
	WIFI_HOSTNAME
	>{
public:
	static constexpr char help[] = "wifi dir";
	static constexpr char name[] = "WIFI_DIR";
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

class MONITOR_AUTOBOOT : public Impl::CElem<MONITOR_AUTOBOOT,UID::MONITOR_AUTOBOOT,Impl::Bool,true>{
public:
	static constexpr bool coldboot_required = true;
	static constexpr char help[] = "Monitor autoboot";
	static constexpr char name[] = "MONITOR_AUTOBOOT";
};

class BiosDir : public Impl::Dir<BiosDir,UID::BIOS_DIR,BIOS_SEGMENT,MONITOR_AUTOBOOT>{
public:
	static constexpr char help[] = "bios dir";
	static constexpr char name[] = "bios Dir";
};

class RootDir : public Impl::Dir<RootDir,UID::ROOT_DIR,UsbDir,WIFI_DIR,BiosDir,MONITOR_AUTOBOOT>{
public:
	static constexpr char help[] = "root_dir";
	static constexpr char name[] = "ROOT_DIR";
};

#ifndef PICOPOCKET_SIM
template<class ... Ts>
using Storage = Impl::FlashSaved<Ts ...>;
#else
template<class ... Ts>
using Storage = Impl::NopSaved<Ts ...>;
#endif
using Flash_Storage = Storage<
		USB_SERIAL_NO,
		WIFI_SSID,
		WIFI_PW,
		WIFI_DHCP,
		WIFI_IP,
		WIFI_MASK,
		WIFI_GW,
		WIFI_DNS,
		WIFI_HOSTNAME,
		BIOS_SEGMENT,
		MONITOR_AUTOBOOT
		>;

using Config = Impl::BasicConfig<Flash_Storage,RootDir>;
using USB_Access = Config;
using WWW_Access = Config;
}
