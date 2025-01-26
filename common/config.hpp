#pragma once
#include "config_classes.hpp"

namespace Config {

#ifndef PICOPOCKET_SIM
template<class ... Ts>
using Storage = Impl::FlashSaved<Ts ...>;
#else
template<class ... Ts>
using Storage = Impl::NopSaved<Ts ...>;
#endif


#define UID_TABLE() \
	enum class UID : uint16_t {
#define UID_TABLE_ENTRY(id) \
	id,
#define UID_TABLE_END() \
	};

#define STR_FIELD(uid, len, def) \
		constexpr char uid ## _DEFAULT[] = def; \
		class uid : public Impl::CElem<uid,UID::uid,Impl::Str<uid,len>,uid ## _DEFAULT> { \
		public:
#define STR_FIELD_END() \
	};
#define HEX_FIELD(uid, type, def) \
		class uid : public Impl::CElem<uid,UID::uid,Impl::HexInt<uid,type>,def> { \
		public:
#define HEX_FIELD_END() \
	};
#define IP_FIELD(uid, def) \
		class uid : public Impl::CElem<uid,UID::uid,Impl::IP<uid>,def> { \
		public:
#define IP_FIELD_END() \
	};

#define BOOL_FIELD(uid, def) \
		class uid : public Impl::CElem<uid,UID::uid,Impl::Bool,def> { \
		public:
#define BOOL_FIELD_END() \
	};

#define DIR_FIELD(uid) \
		class uid : public Impl::Dir<uid,UID::uid
#define FIELD_ENTRY(uid) \
		,uid
#define DIR_FIELD_END_ENTRIES() \
		>{ \
		public:
#define DIR_FIELD_END() \
		};

#define FIELD_COLDBOOT(val) \
		static constexpr bool coldboot_required = val;
#define FIELD_HELP() \
		static constexpr char help[] =
#define FIELD_HELP_END() \
		;
#define FIELD_C_VALIDATE(type) \
		static bool validate(type newval){
#define FIELD_C_VALIDATE_END() \
		};
#define FIELD_NAME(n) \
		static constexpr char name[] = n;
#define FIELD_STORED_TABLE() \
		using Flash_Storage = Storage<
#define FIELD_STORED_TABLE_END() \
		>;
#define STORE_FIELD(uid) \
		,uid
#define STORE_FIELD_FIRST(uid) \
		uid

#include "config.def"

#undef UID_TABLE
#undef UID_TABLE_ENTRY
#undef UID_TABLE_END
#undef STR_FIELD
#undef STR_FIELD_END
#undef HEX_FIELD
#undef HEX_FIELD_END
#undef IP_FIELD
#undef IP_FIELD_END
#undef BOOL_FIELD
#undef BOOL_FIELD_END
#undef FIELD_COLDBOOT
#undef FIELD_HELP
#undef FIELD_HELP_END
#undef FIELD_C_VALIDATE
#undef FIELD_C_VALIDATE_END
#undef FIELD_NAME
#undef DIR_FIELD
#undef FIELD_ENTRY
#undef DIR_FIELD_END_ENTRIES
#undef DIR_FIELD_END
#undef FIELD_STORED_TABLE
#undef FIELD_STORED_TABLE_END
#undef STORE_FIELD_FIRST
#undef STORE_FIELD

using Config = Impl::BasicConfig<Flash_Storage,ROOT_DIR>;
using USB_Access = Config;
using WWW_Access = Config;
using IO_Access = Config;
}
