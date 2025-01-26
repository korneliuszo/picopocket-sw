#pragma once

#include<cstdint>
#include<stdlib.h>
#include<cstdio>
#include<limits>
#include<tuple>
#include<cstring>

#include "constexprstring.hpp"
#ifndef PICOPOCKET_SIM
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#endif

namespace Config {

namespace Impl {

template
<typename R, class C, auto F, typename ... Args>
static R CWrap(void * v_obj, Args ... args)
{
	C * that = reinterpret_cast<C*>(v_obj);
	return (that->*F)(args...);
}

template < typename T >
constexpr T  max ( const T & a )
{
	return a ;
}
template < typename T , typename ... Args >
constexpr T  max ( const T & a , const T & b , const Args &... args )
{
		return max( b > a ? b : a , args ...);
}


template
<typename R, class C, auto F, typename ... Args>
static R CWrap(const void * v_obj, Args ... args)
{
	const C * that = reinterpret_cast<const C*>(v_obj);
	return (that->*F)(args...);
}


class NoSetRepr {
public:
	bool set_repr(const char* str)
	{
		return false;
	}
};

template<class CRTP,auto EnumVal, class ... Entries>
class Dir {
public:

	class DirElem {
	public:
		using Es = decltype(std::tuple_cat(typename Entries::E{} ...));
		static constexpr auto uid = EnumVal;
		static constexpr uint16_t uid16 = static_cast<uint16_t>(EnumVal);
		static constexpr bool coldboot_required = false;
		static constexpr const char * help = CRTP::help;
		static constexpr const char * name = CRTP::name;
		static constexpr bool is_directory = true;
		class TVAL : public NoSetRepr {
		public:
			static constexpr std::size_t MAX_STRLEN = sizeof...(Entries)*sizeof(uint16_t);

			template<typename ... Ts>
			struct uid_container {
				static constexpr uint16_t table[sizeof...(Ts)] =
				{
								{Ts::uid16} ...
				};
			};

			template <typename ... Ts>
			static uid_container<Ts...> deTuple (std::tuple<Ts...>);

			using uid_table = decltype(deTuple(Es{}));

			std::size_t repr(char ret[MAX_STRLEN]) const
			{
				memcpy(ret,uid_table::table,MAX_STRLEN);
				return MAX_STRLEN;
			}
		};
		static inline TVAL val;
		static void reset() {};
	};
	using E = std::tuple<DirElem>;
	using Ea = decltype(std::tuple_cat(E{},typename Entries::Ea{}...));
};

class Bool{
public:
	bool ival;
	constexpr Bool(bool defval): ival(defval){};
	using T = bool;
	static constexpr std::size_t MAX_STRLEN = 2;


	std::size_t repr(char ret[MAX_STRLEN]) const
		{
		strncpy(ret,ival?"T":"F",MAX_STRLEN);
		return strlen(ret);
	}

	bool set_repr(const char* str)
	{
		ival = str[0] == 'T';
		return true;
	}
};

template<typename UINT>
class HexInt_bottom{
public:
	UINT ival;
	constexpr HexInt_bottom(UINT defval): ival(defval){};
	using T = UINT;
	static constexpr std::size_t MAX_STRLEN = sizeof(UINT)*2+3;


	std::size_t repr(char ret[MAX_STRLEN]) const
		{
		static constexpr auto str = CStrcat(
				CString("0x%0"),
				fxdintz<3>(sizeof(UINT)*2),
				CString("X"),
				CString("\0"));
		snprintf(ret,MAX_STRLEN,str.data(),ival);
		return strlen(ret);
	}
};
template<class CRTP, typename UINT>
class HexInt : public HexInt_bottom<UINT>{
public:
	constexpr HexInt(UINT defval): HexInt_bottom<UINT>(defval){};
	using T = UINT;
	static constexpr std::size_t MAX_STRLEN = HexInt_bottom<UINT>::MAX_STRLEN;

	bool set_repr(const char* str)
	{
		UINT tmpval = strtoul(str,nullptr,16);
		if(CRTP::validate(tmpval))
		{
			HexInt_bottom<UINT>::ival = tmpval;
			return true;
		}
		return false;
	}
};

template<typename UINT>
class UInt_bottom{
public:
	UINT ival;
	constexpr UInt_bottom(UINT defval): ival(defval){};
	using T = UINT;
	static constexpr std::size_t MAX_STRLEN = 12;
	std::size_t repr(char ret[MAX_STRLEN]) const
		{
		snprintf(ret,MAX_STRLEN,"%u",ival);
		return strlen(ret);
	}
};
template<class CRTP, typename UINT>
class UInt : public UInt_bottom<UINT>{
public:
	constexpr UInt(UINT defval): UInt_bottom<UINT>(defval){};
	using T = UINT;
	static constexpr std::size_t MAX_STRLEN = UInt_bottom<UINT>::MAX_STRLEN;

	bool set_repr(const char* str)
	{
		UINT tmpval = strtoul(str,nullptr,0);
		if(CRTP::validate(tmpval))
		{
			UInt_bottom<UINT>::ival = tmpval;
			return true;
		}
		return false;
	}
};


class IP_bottom{
public:
	uint32_t ival;
	constexpr IP_bottom(uint32_t defval): ival(defval){};
	using T = uint32_t;
	static constexpr std::size_t MAX_STRLEN = 4*4;


	std::size_t repr(char ret[MAX_STRLEN]) const
		{
		snprintf(ret,MAX_STRLEN,"% 3u.% 3u.% 3u.% 3u",
				0xFF&ival,
				0xFF&(ival>>8),
				0xFF&(ival>>16),
				0xFF&(ival>>24));
		return strlen(ret);
	}
	void set_repr(uint32_t &out, const char* str)
	{
		  char * pEnd;
		  out = 0;
		  out |= strtol(str,&pEnd,10);
		  if(*pEnd == '.') pEnd++;
		  out |= strtol(pEnd,&pEnd,10)<<8;
		  if(*pEnd == '.') pEnd++;
		  out |= strtol(pEnd,&pEnd,10)<<16;
		  if(*pEnd == '.') pEnd++;
		  out |= strtol(pEnd,&pEnd,10)<<24;
	}
};
template<class CRTP>
class IP : public IP_bottom{
public:
	constexpr IP(uint32_t defval): IP_bottom(defval){};
	using T = uint32_t;
	static constexpr std::size_t MAX_STRLEN = IP_bottom::MAX_STRLEN;

	bool set_repr(const char* str)
	{
		uint32_t tmpval;
		IP_bottom::set_repr(tmpval,str);
		if(CRTP::validate(tmpval))
		{
			IP_bottom::ival = tmpval;
			return true;
		}
		return false;
	}
};

template<class CRTP, std::size_t LEN>
class Str {
public:
	char ival[LEN];
	constexpr Str(const char * defval) : ival(){
		char * it = ival;
		while(*defval)
		{
			*it++ = *defval++;
		}
		//zeroed memory anyway
		//*it++ = *defval++;
	};
	using T = char[LEN];
	static constexpr std::size_t MAX_STRLEN = LEN;


	std::size_t repr(char ret[MAX_STRLEN]) const
		{
		strncpy(ret,ival,MAX_STRLEN);
		return strlen(ret);
	}

	bool set_repr(const char* istr)
	{
		std::size_t len = strnlen(istr,MAX_STRLEN+1);
		if(len == MAX_STRLEN+1)
			return false;
		if(CRTP::validate(istr))
		{
			strncpy(ival,istr,MAX_STRLEN);
			return true;
		}
		return false;
	}
};


template<class CRTP, auto EnumVal, class VAL, auto const  DEFAULT>
class CElem {
public:
	static constexpr auto uid = EnumVal;
	static constexpr uint16_t uid16 = static_cast<uint16_t>(EnumVal);
	using TVAL = VAL;
	using E = std::tuple<CRTP>;
	using Ea = E;
	using Es = std::tuple<>;
	static inline VAL val = DEFAULT;
	static constexpr bool is_directory = false;
	static void reset() {val = DEFAULT;};
};

#ifndef PICOPOCKET_SIM
namespace {
extern "C" uint8_t const __flash_save_start[];
extern "C" uint8_t const __flash_save_end[];
}

template<class ... Flash_Config>
class FlashSaved {
private:
	struct 	[[gnu::packed]] Entry {
		uint32_t size;
		uint16_t uid;
		uint8_t val[0];
	};

	struct [[gnu::packed]] Header {
		static constexpr uint32_t magic_val = 0x4E797502;
		uint32_t magic = magic_val;
		uint8_t entries[0];
	};
	static constexpr std::size_t SECTOR_SIZE = 4*1024;
	static const uint8_t * find_last()
	{
		bool last_sector_clear = false;
		const uint8_t * last_sector = __flash_save_end-SECTOR_SIZE;
		for(const uint8_t*sector = __flash_save_start;sector<__flash_save_end;sector+=SECTOR_SIZE)
		{
			bool sector_clear = true;
			for(const uint8_t * val = sector;val<sector+SECTOR_SIZE;val++)
				if(*val !=0xff)
				{
					sector_clear =false;
					break;
				}
			if(sector_clear)
				break;
			last_sector=sector;
		}
		return last_sector;
	}

	static const uint8_t * find_next_sector(const uint8_t * last_sector)
	{
		last_sector+= SECTOR_SIZE;
		if(last_sector >=__flash_save_end)
			last_sector=__flash_save_start;
		return last_sector;
	}

	struct SavedFields {
		const uint16_t uid;
		void * const ival;
		const size_t size;

		template<class Conf>
		constexpr
		SavedFields(const Conf & conf)
		: uid(Conf::uid16)
		, ival(static_cast<void*>(&Conf::val.ival))
		, size(sizeof(Conf::val.ival))
		{}
	};

	using save_tuple = decltype(std::tuple_cat(typename Flash_Config::E{}...));

	template<typename ... Args>
	struct SavedTable_Impl
	{
		static constexpr SavedFields table[] = {{SavedFields(Args{})}...};
	};

	template <typename ... Ts>
	static SavedTable_Impl<Ts...> deTuple (std::tuple<Ts...>);

	using SavedTable = decltype(deTuple(save_tuple{}));

	class PingPongstream{
		uint8_t buff[2][256];
		std::size_t off = 0;
		std::size_t pp_sel = 0;
		uint32_t foff;
	public:
		PingPongstream(uint32_t _foff) : foff(_foff){};
		void push(const uint8_t* data, std::size_t len)
		{
			if(off+len >= 256)
			{
				std::size_t tolen = 256 - off;
				memcpy(&buff[pp_sel][off],data,tolen);
				data+=tolen;
				len-=tolen;
				flash_range_program(foff,buff[pp_sel],256);
				pp_sel = (pp_sel+1)%2;
				foff+=256;
				off=0;
			}
			if(len)
			{
				memcpy(&buff[pp_sel][off],data,len);
				off+=len;
			}
		}
		void finalize()
		{
			Entry entry = {std::numeric_limits<uint32_t>::max()};
			push((uint8_t*)&entry, sizeof(entry));
			std::size_t tolen = 256 - off;
			memset(&buff[pp_sel][off],0xf0,tolen);
			flash_range_program(foff,buff[pp_sel],256);
		}
	};

	static constexpr std::size_t savesize()
	{
		return ((sizeof(Flash_Config::val.ival)+sizeof(Entry))+ ... ) + sizeof(Header);
	}
	static_assert(savesize() < SECTOR_SIZE);
public:
	typedef std::tuple<Flash_Config ...> Saved;
	static void load()
	{
		Header * readout = (Header*)find_last();
		if(readout->magic == Header::magic_val)
		{
			Entry * entry = (Entry*)readout->entries;
			while(entry->size < SECTOR_SIZE - (entry - (Entry*)readout->entries))
			{
				for(const SavedFields& ielem : SavedTable::table)
				{
					if(entry->uid == ielem.uid)
					{
						memcpy(ielem.ival,entry->val,ielem.size);
						break;
					}
				}
				entry = (Entry*)(((uint8_t*)entry)+entry->size);
			}
		}
	}

	static void save()
	{
		const uint8_t * last_sector = find_last();
		const uint8_t * next_sector = find_next_sector(last_sector);
		const uint8_t * erase_sector = find_next_sector(next_sector);

		flash_range_erase(uint32_t(erase_sector-XIP_BASE),SECTOR_SIZE);

		PingPongstream writer = {uint32_t(next_sector-XIP_BASE)};
		Header hdr = {};
		writer.push((uint8_t*)&hdr,sizeof(hdr));
		for(const SavedFields& ielem : SavedTable::table)
		{
			auto size = ielem.size+sizeof(Entry);
			auto pad_size = (size+3)/4*4;
			Entry elem = {pad_size,ielem.uid};
			writer.push((uint8_t*)&elem,sizeof(Entry));
			writer.push((uint8_t*)ielem.ival,ielem.size);
			uint8_t pad[] = {0x0f,0x0f,0x0f,0x0f};
			writer.push(pad,pad_size-size);
		}
		writer.finalize();
	}
};
#endif

template<class ... Flash_Config>
class NopSaved {
private:

public:
	typedef std::tuple<Flash_Config ...> Saved;
	static void load()
	{
	}

	static void save()
	{
	}
};

template<class Saved, class Basic_Config>
class BasicConfig {
public:
	typedef Saved TSaved;

private:
	template<class ... Ts>
	static constexpr std::size_t MAX_MAX_STRLEN_fn(std::tuple<Ts...> _)
	{
		return max(Ts::TVAL::MAX_STRLEN ...);
	}
	template<class ... Ts>
	static constexpr std::size_t FIELD_COUNT_fn(std::tuple<Ts...> _)
	{
		return sizeof ... (Ts);
	}

	template<class Entry, class ... Ts>
	static constexpr bool is_inside_tuple(Entry _1,std::tuple<Ts...> _2)
	{
		return ((std::is_same<Entry,Ts>::value == true) || ... || false);
	}

	template<class Entry, class Parent>
	static constexpr bool is_parent(uint16_t &ret,Entry _1,Parent _2)
	{
		if(is_inside_tuple(Entry{}, typename Parent::Es{}))
		{
			ret = Parent::uid16;
			return true;
		}
		return false;
	}

	template<class Entry, class ... Ts>
	static constexpr uint16_t parent_fn(Entry _1,std::tuple<Ts...> _2)
	{
		uint16_t ret = 0;
		(is_parent(ret,Entry{},Ts{}) || ...);
		return ret;
	}
public:
	static constexpr std::size_t MAX_MAX_STRLEN =
			MAX_MAX_STRLEN_fn(typename Basic_Config::Ea{});

	static constexpr std::size_t FIELD_COUNT =
			FIELD_COUNT_fn(typename Basic_Config::Ea{});


	struct ConfigFields {
		const uint16_t uid;
		const uint16_t parent_uid;
		const bool coldboot_required;
		const bool to_flash;
		const bool is_directory;
		const char * const help;
		const char * const name;
		void * const val;
		std::size_t (*const repr)(const void * v_obj, char [MAX_MAX_STRLEN]);
		bool (*const set_repr)(void * v_obj, const char * str);
		void (*const reset)();

		template<class Entry>
		constexpr ConfigFields(const Entry & _)
		: uid(Entry::uid16)
		, parent_uid(parent_fn(Entry{},typename Basic_Config::Ea{}))
		, coldboot_required(Entry::coldboot_required)
		, to_flash(is_inside_tuple(Entry{},typename TSaved::Saved{}))
		, is_directory(Entry::is_directory)
		, help(Entry::help)
		, name(Entry::name)
		, val(static_cast<void*>(&Entry::val))
		, repr(&CWrap<std::size_t,decltype(Entry::val),&decltype(Entry::val)::repr>)
		, set_repr(&CWrap<bool,decltype(Entry::val),&decltype(Entry::val)::set_repr>)
		, reset(&Entry::reset)
		{};
	};
private:
	template<class ... Args>
	class ConfigFields_Impl {
	public:
		static constexpr ConfigFields table[] = {
			{
				ConfigFields{Args{}}
			}...
		};
	};

	template <typename ... Ts>
	static ConfigFields_Impl<Ts...> deTuple (std::tuple<Ts...>);

	using ConfigElems = decltype(deTuple(typename Basic_Config::Ea{}));

public:
	static const ConfigFields* uid(uint16_t uid)
	{
		for(const ConfigFields & elem : ConfigElems::table)
		{
			if(elem.uid == uid)
				return &elem;
		}
		return nullptr;
	}
};

}
}
