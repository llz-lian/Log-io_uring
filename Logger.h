// Logger.h: 标准系统包含文件的包含文件
// 或项目特定的包含文件。
#pragma once
#include <string>
#include <map>
#include <thread>
#include <semaphore>
#include <format>
#include <functional>
#include <iostream>

#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include"Buffer.h"
#include"json/Json.h"
// load json
const char* config_path = "./config.json";
inline Json json;

namespace Logger
{
	namespace
	{
		using Msg = std::string;
		struct LogConfig : protected CanJson
		{
			enum class LOG_TYPE
			{
				ASYNC,
				SYNC
			};
			enum class LOG_LEVEL
			{
				ERROR,
				WARN,
				INFO,
				DEBUG,
				TRACE
			};
#define CHECK(STR) if(str == #STR) return LOG_LEVEL::STR;
			enum class LOG_OUTPUT
			{
				None = 0,
				CONSOLE = 0b01,
				FILE = 0b10
			};
#define COMB(STR) if(sub == #STR) ret |= (uint8_t)LOG_OUTPUT::STR;
			LOG_LEVEL getLogLevel(const std::string& str)
			{
					CHECK(DEBUG)
					CHECK(WARN)
					CHECK(INFO)
					CHECK(DEBUG)
					CHECK(TRACE)
					return LOG_LEVEL::ERROR;
			}
			uint8_t getOutputLevel(const std::string& str)
			{
				std::stringstream ss(str);
				std::string sub;
				uint8_t ret = 0;
				while (ss >> sub)
				{
					COMB(FILE);
					COMB(CONSOLE);
				}
				return 0;
			}
			Json toJson() const { return Json{}; }// not use
			LogConfig(const Json& json)
				:buffer(atoi(std::string(json["log_buffersize"]).data()))
			{
				checkUpdate();
			}
			void checkUpdate()
			{
				// check config updated
				json.read(config_path);
				log_name = std::string(json["log_name"]);
				log_path = std::string(json["log_path"]);
				output = getOutputLevel(std::string(json["log_output"]));
				level = getLogLevel(std::string(json["log_level"]));
				file_max_size = atoi(std::string(json["log_fileMaxsize"]).data());
			}
			std::string log_name;// name_xxx_yyy_zzz
			std::string log_path;// abs path
			uint8_t output;
			LOG_LEVEL level;
			size_t file_max_size;// byte
			Buffer<Msg> buffer; // can not change size
		};
		inline std::binary_semaphore sem(0);
	}
};


namespace Logger
{
	// file control
	// file stream or fd
	// file meta data
	namespace Writer
	{
		inline int fd = 0;
		inline int num = 0;

		inline std::string file_name;
		inline struct ::stat file_stat;

		size_t buffered_size = 0;
		const size_t max_buffered_size = 3950;
		// std::string self_buffer;
		std::vector<std::string> self_buffer;

		size_t fsync_cnt = 0;
		const size_t fsync_times = 10;
		void checkFileName()
		{
			// check if can write
			// if size >= max_size switch to next file
			auto ret = ::stat(file_name.data(),&file_stat);
			auto config = Config::get();
			while(ret != 0 || file_stat.st_size > config.file_max_size)
			{
				// move to next file
				num++;
				file_name = std::format("{}/{}_{}_{}",config.log_path,config.log_name, std::this_thread::get_id(), num);
				ret = ::stat(file_name.data(),&file_stat);
			}
			// creat new write only
			if(::errno == 2)
			{
				fd = ::open(file_name.data(),O_CREAT|O_WRONLY,S_IRUSR | S_IWUSR);
			}
			else if(fd == 0)
			{
				fd = ::open(file_name.data(),O_WRONLY);
			}
		}
		void writeAll(bool sync)
		{
			// write data
			for(auto & data:self_buffer)
			{
				::write(fd,data.data(),data.size());
			}
			// ::write(fd,self_buffer.data(),self_buffer.size());
			self_buffer.clear();
			if(sync) ::fsync(fd);
			checkFileName();
			return;
		}
		void write(const Msg & msg)
		{
			if(buffered_size>= max_buffered_size)
			{
				if(fsync_cnt>=fsync_times)
				{
					writeAll(true);
					fsync_cnt = 0;
				}
				else
					writeAll(false);
				fsync_cnt++;
				buffered_size = 0;
				return;
			}
			// buffer data
			buffered_size += msg.size() + 1;
			// self_buffer += msg;// copy but continuous, memory need it?
			self_buffer.push_back(std::move(msg));// not continuous， but no copy
			self_buffer.back().push_back('\n');
		}
		void initFileName()
		{
			// init file_name
			auto& config = Config::get();
			file_name = std::format("{}/{}_{}_{}",config.log_path,config.log_name, std::this_thread::get_id(), num);
			checkFileName();
			self_buffer.reserve(100);
		}
	}
};

namespace Logger
{
	namespace // worker
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
			if(!(output & LOG_OUTPUT::None))
			{
				if(output & LOG_OUTPUT::CONSOLE)
				{
					std::cout<<data<<std::endl;
				}
				if(output & LOG_OUTPUT::FILE)
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
								::fsync(fd);
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
	}
};
namespace Logger
{
	// file_name_[LOGGER-UID][LOGGER-PID][LOGGER-TID]_num
	//[level]: time  log infos
	class Config
	{
		LogConfig __config;
		Config(const Json& json) :__config(json) {}
		Config(Config&) = delete;
		Config(const Config&) = delete;
		Config& operator=(const Config&) = delete;
	public:
		~Config()
		{
			// write all buffered data
			Writer::writeAll(true);			
		}
		static LogConfig & get()
		{
			static Config config(json);
			return config.__config;
		}
	};
	namespace
	{
		using handle = std::function<void(const Msg & msg)>;
		// call DEBUG(msg) ==> log(handles[LOG_LEVEL::DEBUG](msg))
		std::vector<handle> handles(5);// continous
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
		// build real msg
		inline Msg Debug(const Msg & msg)
		{
			return std::format("");
		}
		inline Msg Info(const Msg & msg)
		{
			return std::format("");
		}
		inline Msg Warnning(const Msg & msg)
		{
			return std::format("");
		}
		inline Msg empty(const Msg & msg){return ""};
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
}
// TODO: 在此处引用程序需要的其他标头。