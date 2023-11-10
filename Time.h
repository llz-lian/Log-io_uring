#include <chrono>
#include <ctime>
#include <fmt/core.h>
namespace Time
{
	using namespace std::chrono;
	std::string getNowTime()
	{
		auto time = system_clock::to_time_t(system_clock::now());
		auto now_time = gmtime(&time);
		return fmt::format("{}/{}/{} {}:{}:{:02}",
			now_time->tm_year + 1900, now_time->tm_mon + 1, now_time->tm_mday, 
			now_time->tm_hour, now_time->tm_min, now_time->tm_sec);
	}
}