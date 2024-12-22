#pragma once
#include "fifo.hpp"
#include <algorithm>

template<typename T,int SIZE>
class FIFO_WITH_BRK: public Fifo<T,SIZE>
{
private:
	typename Fifo<T,SIZE>::Iterator brkptr;
public:
	FIFO_WITH_BRK(): Fifo<T,SIZE>(),brkptr(Fifo<T,SIZE>::mr){
		brkptr.ptr = nullptr;
	};

	inline void clear() volatile
	{
		brkptr.ptr = nullptr;
		Fifo<T,SIZE>::clear();
	}

	inline void fifo_put_brk() volatile
	{
		brkptr.ptr = Fifo<T,SIZE>::wrptr.ptr;
	}

	inline typename Fifo<T,SIZE>::Iterator::Dereference get_rdbuff_to_brk()
	{
		typename Fifo<T,SIZE>::Iterator::Dereference buff = {Fifo<T,SIZE>::get_rdbuff()};
		buff.set_len(std::min(brk_check(),buff.get_len()));
		return buff;
	}

	inline size_t brk_check() volatile
	{
		std::make_signed<size_t>::type ret = (brkptr.ptr - Fifo<T,SIZE>::rdptr.ptr);
		if (ret < 0) ret +=SIZE;
		return ret;
	}

	inline void commit_rdbuff(size_t len) volatile
	{
		if(is_brk_in_queue() && (brk_check() <=len))
			brkptr.ptr = nullptr;
		Fifo<T,SIZE>::rdptr.advance(len);
	}

	inline bool is_brk_in_queue() volatile
	{
		return !!brkptr.ptr;
	}
	inline bool is_brk_on_top() volatile
	{
		return brkptr.ptr == Fifo<T,SIZE>::rdptr.ptr;
	}
};
