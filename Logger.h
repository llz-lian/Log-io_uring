// Logger.h: 标准系统包含文件的包含文件
// 或项目特定的包含文件。
#pragma once


#include"Writer.h"
#include"Buffer.h"
#include"json/Json.h"
// load json

namespace Logger
{
	const size_t wait_ms = 100;
	size_t passed_time = 0;
	const size_t passed_sync_time = 10;
	inline void work()
	{
		// fetch data
		auto data = Config::get().buffer.pop();// throw when empty
		// check output
		auto output = Config::get().output;
		if(!(output & (uint8_t)LOG_OUTPUT::None))
		{
			if(output & (uint8_t)LOG_OUTPUT::CONSOLE)
			{
				std::cout<<data<<std::endl;
			}
			if(output & (uint8_t)LOG_OUTPUT::FILE)
			{
				// send to writer 
				// data moved
				Writer::write(data);
			}
		}
	}
	inline void awakeWorker()
	{
		sem.release();
	}
	inline bool stunWorker()
	{
		// wait 50ms
		return sem.try_acquire_for(std::chrono::milliseconds(wait_ms));// or 50ms
	}
	inline void initWorker() noexcept
	{
		std::jthread worker{[&]() {
			bool has_data = false;
			while (true)
			{
				// fetch data
				try
				{
					if(has_data)
					{
						while (true) work();
					}
					else
					{
						passed_time++;
						if(passed_time >= passed_sync_time)
						{
							passed_time = 0;
							::fsync(Writer::fd);
						}
					}
				}
				catch (const std::exception&) 
				{
					has_data = stunWorker();// no data sleep xx ms
				}
			}
		}
		};
		// worker.detach(); jthread
	}
// file_name_[LOGGER-UID][LOGGER-PID][LOGGER-TID]_num
//[level]: time  log infos
	using handle = std::function<void(const Msg &)>;
	// call DEBUG(msg) ==> log(handles[LOG_LEVEL::DEBUG](msg))
	std::vector<handle> handles(5);// continous
	// build real msg
	inline Msg Debug(const Msg & msg)
	{
		return fmt::format("");
	}
	inline Msg Info(const Msg & msg)
	{
		return fmt::format("");
	}
	inline Msg Warnning(const Msg & msg)
	{
		return fmt::format("");
	}
	inline Msg empty(const Msg & msg){return "";}
	void initHandles(LOG_LEVEL level)
	{
		for(size_t i = 0;i<5;i++)
		{
			handles[i] = empty;
		}
		// use level build handles
		switch (level)
		{
			// add handle
			case LOG_LEVEL::DEBUG:
				break;
			case LOG_LEVEL::WARN:
				break;
			case LOG_LEVEL::INFO:
				break;
		}
	}
	inline void log(const Msg& msg)
	{
		if(msg.empty()) return;
		// push to buffer
		auto& buffer = Config::get().buffer;
		while(!buffer.push(msg))
		{
			// awakeWorker for push
			awakeWorker();
		}
		// awake worker for push
		awakeWorker();
	}

	inline void init()
	{
		// load config file
		// init json & config
		json.read(config_path);
		initHandles(Config::get().level);
		Writer::initFileName();
		// creat worker thread
		initWorker();
	}
	// TODO: 在此处引用程序需要的其他标头。
};
