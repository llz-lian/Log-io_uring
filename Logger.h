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

		inline size_t write_cnt = 0;
		inline std::string file_name;
		inline struct ::stat file_stat;

		size_t buffered_size = 0;
		size_t max_buffered_size = 3950;
		std::string self_buffer;

		size_t fsync_cnt = 0;
		size_t fsync_times = 10;
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
			::write(fd,self_buffer.data(),self_buffer.size());
			if(sync) ::fsync(fd);
			checkFileName();
			self_buffer.clear();
			return;
		}
		void write(const Msg & msg)
		{
			if(self_buffer.size()>= self_buffer)
			{
				if(fsync_cnt>=fsync_times)
				{
					writeAll(true);
					fsync_cnt = 0;
				}
				else
					writeAll(false);
				fsync_cnt++;
				return;
			}
			// buffer data
			self_buffer += std::move(msg);// copy but cotinuous, memory need it?
			self_buffer.push_back('\n');
		}
		void initFileName()
		{
			// init file_name
			auto& config = Config::get();
			file_name = std::format("{}/{}_{}_{}",config.log_path,config.log_name, std::this_thread::get_id(), num);
			checkFileName();
			self_buffer.reserve(4096);
		}
	}
};

namespace Logger
{
	namespace // worker
	{
		inline void work()
		{
			// fetch data
			auto data = Config::get().buffer.pop();
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
		inline void stunWorker()
		{
			sem.acquire();
		}
		inline void initWorker() noexcept
		{
			std::jthread worker{[&]() {
				while (true)
				{
					// fetch data
					try
					{
						while (true)
							work();
					}
					catch (const std::exception&) // no data sleep
					{
						stunWorker();
					}
				}
			}
			};
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
		using handle = std::function<void(const Mst & msg)>;
		std::vector<handle> handles(5);// continous
		void initHandles(LOG_LEVEL level)
		{
			for(size_t i = 0;i<5;i++)
			{
				handles[i] = empty;
			}
			switch (level)
			{
				// add handle
				case LOG_LEVEL::DEBUG:
					break;
				case LOG_LEVEL::WARN:
					break;
			}
		}
		inline void log(const Msg& msg)
		{
			// push to buffer
			auto& config = Config::get();
			auto& buffer = config.buffer;
			while(!buffer.push(msg)) handle[size_t(LOG_LEVEL::WARN)]();
			// awake worker
			awakeWorker();
		}
		inline void Debug(const Msg & msg)
		{

		}
		inline void Info(const Msg & msg)
		{

		}
		inline void Warnning(const Msg & msg)
		{

		}
		inline void empty(const Msg & msg){};
	}
	inline void init()
	{
		// load config file
		// init json & config
		json.read(config_path);
		Config::get();
		Writer::initFileName();
		// creat worker thread
		initWorker();
	}
}
// TODO: 在此处引用程序需要的其他标头。