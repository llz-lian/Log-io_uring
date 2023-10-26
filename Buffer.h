#include <atomic>
#include <vector>
#include <stdexcept>
// one read one write lock free buffer
// note that push & move not keep data!
// 

//#define DEBUG
template<class T>
class Buffer
{
	// ring buffer
	// lock free
	std::vector<T> __datas;
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
		auto size = (head - tail) % (n + 1);
		if (size == n)
		{
			// full
			return false;
		}
		__datas[head % n] = std::move(val);
		// update head
		__head.store(head + 1, std::memory_order_release);
		return true;
	}
	T pop()
	{
		auto n = __datas.size();
		auto head = __head.load(std::memory_order_relaxed);
		auto tail = __tail.load(std::memory_order_acquire);
		auto size = (head - tail) % (n + 1);
		if (size == 0)
		{
			throw std::runtime_error("pop an empty queue");
		}
		auto ret = std::move(__datas[tail % n]);// need be here
		__tail.store(tail + 1, std::memory_order_release);
		// need move here
		// means
		// return T(std::move(__datas[tail]));
		return ret;
	}
};