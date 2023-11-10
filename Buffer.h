#pragma once
#include <atomic>
#include <vector>
#include <stdexcept>
#include <iostream>
template<class T>
class Buffer
{
	// ring buffer
	// lock free
	struct alignas(64) Value
	{
		T t;
	};
	std::vector<Value> __datas;
	std::atomic<size_t> __head; // next insert pos
	std::atomic<size_t> __tail; // next read pos
public:
	Buffer(size_t size)
	{
		if (size == 0)
		{
			throw std::runtime_error("size can not be 0!");
		}
		__datas.resize(size);// call T()
		__head = __tail = 0;
	}
	template<class U>
	bool push(U&& val)
	{
		auto n = __datas.size();
		auto tail = __tail.load(std::memory_order_relaxed);
		auto head = __head.load(std::memory_order_acquire);
		auto size = (head - tail) % (n+1);
		if (size == n)
		{
			// full
			return false;
		}
		__datas[head % n].t = std::move(val);
		// update head
		__head.store(head + 1, std::memory_order_release);
		return true;
	}
	T pop()
	{
		auto n = __datas.size();
		auto head = __head.load(std::memory_order_relaxed);
		auto tail = __tail.load(std::memory_order_acquire);
		auto size = (head - tail) % (n+1);
		if (size == 0)
		{
			throw std::runtime_error("pop an empty queue");
		}
		auto ret = std::move(__datas[tail % n].t);// need be here
		__tail.store(tail + 1, std::memory_order_release);
		// need move here
		// means
		// return T(std::move(__datas[tail]));
		return ret;
	}
};