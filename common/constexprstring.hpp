#pragma once

#include<cstdint>
#include<array>

template<std::size_t LEN, char ... chars>
class CStr : public std::array<char,LEN>{
public:
	constexpr CStr(const char in[LEN+1])
	: std::array<char,LEN>(in){}

	constexpr CStr()
	: std::array<char,LEN>(){}
};

template<std::size_t LEN, typename UINT>
constexpr CStr<LEN> fxdintz(UINT v)
{
	CStr<LEN> ret = {};
	UINT msb = 1;
	for(std::size_t i=0;i<LEN;i++)
		msb*=10;
	for(std::size_t i=0;i<LEN;i++)
	{
		msb=msb/10;
		ret[i] = '0' + (v / msb);
		v = v % msb;
	}
	return ret;
}

template <std::size_t LEN>
constexpr CStr<LEN-1> CString(const char (&in)[LEN])
{
	CStr<LEN-1> ret = {};
	for(std::size_t i=0;i<LEN-1;i++)
		ret[i] = in[i];
	return ret;
}

template<std::size_t ... LEN>
constexpr CStr<(LEN + ...)> CStrcat(const CStr<LEN>& ... parts)
{
	CStr<(LEN + ...)>  result = {};
    std::size_t index = 0;

    ([&result,&index](const auto & part){
    	for(std::size_t i=0;i<LEN;i++)
    		result[index++] = part[i];
    }(parts), ... );

	return result;
}
