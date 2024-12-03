#pragma once

template <typename T, int SIZE>
class Fifo
{
protected:
	volatile T data[SIZE];

	using Tp =  volatile T *;

	struct MemoryRegion {
		const Tp data_start;
		const Tp data_end;
	};

	const MemoryRegion mr = {data, &data[SIZE]};

	struct Iterator {
		struct Dereference {
			Tp start;
			Tp stop;
			Tp start2;
			Tp stop2;
			Dereference(const Iterator it)
			: start(it.ptr)
			, stop(it.mr.data_end)
			, start2(it.mr.data_start)
			, stop2(it.ptr)
			{};
			void set_len(ssize_t len)
			{
				len -= stop-start;
				if (len < 0)
				{
					stop2=start2;
					stop = stop + len;
				}
				else {
					stop2 = start2 + len;
				}
			}
			Tp operator [](size_t idx) volatile
			{
				ssize_t idx2 = idx-(stop-start);
				if(idx2<0)
					return &start[idx];
				else
					return &start2[idx2];
			}
			void put_bytes(const T* buff, ssize_t len,size_t at)
			{
				ssize_t len1 = len-(stop-(start+at));
				if(len1<=0)
				{
					memcpy(const_cast<T*>(&start[at]),buff,len);
				}
				else
				{
					ssize_t at2 = at-(stop-start);
					if(at2 > 0)
					{
						memcpy(const_cast<T*>(&start2[at2]),buff,len);
					}
					else
					{
						memcpy(const_cast<T*>(&start[at]),buff,len1);
						memcpy(const_cast<T*>(start2),&buff[len1],len-len1);
					}
				}
			}
		};
		volatile Tp ptr;
		const MemoryRegion & mr;

		Iterator(const MemoryRegion & _mr) : ptr(_mr.data_start), mr(_mr)
		{
		}
		Iterator(const Iterator & _mr) : ptr(_mr.ptr), mr(_mr.mr)
		{
		}
		inline void advance(size_t len) volatile
		{
			Tp tm = ptr+len;
			if(tm >= mr.data_end)
				tm -= mr.data_end-mr.data_start;
			ptr = tm;
		}
		inline void retreat(size_t len) volatile
		{
			Tp tm = ptr-len;
			if(tm < mr.data_start)
				tm =+ mr.data_end-mr.data_start;
			ptr = tm;
		}
	};

	Iterator wrptr;
	Iterator rdptr;

public:
	Fifo()
	: wrptr(mr)
	, rdptr(mr)
	{};

	inline void clear() volatile
	{
		wrptr.ptr = data;
		rdptr.ptr = data;
	}

	inline typename Iterator::Dereference get_wrbuff()
	{
		typename Iterator::Dereference buff = {wrptr};
		buff.set_len(fifo_free());
		return buff;
	}

	inline void commit_wrbuff(size_t len) volatile
	{
		wrptr.advance(len);
	}

	inline typename Iterator::Dereference get_rdbuff()
	{
		typename Iterator::Dereference buff = {rdptr};
		buff.set_len(fifo_check());
		return buff;
	}

	inline void commit_rdbuff(size_t len) volatile
	{
		rdptr.advance(len);
	}

	inline bool is_fifo_full()
	{
		Iterator tmrd = wrptr;
		tmrd.advance();
		return tmrd.ptr == rdptr.ptr;
	}

	inline bool is_fifo_empty()
	{
		return rdptr.ptr == wrptr.ptr;
	}

	inline long fifo_check()
	{
		int ret = (wrptr.ptr - rdptr.ptr);
		if (ret < 0) ret +=SIZE;
		return ret;
	}

	inline long fifo_free()
	{
		return SIZE-1-fifo_check();
	}
};
