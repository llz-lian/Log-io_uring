// Logger.h: 标准系统包含文件的包含文件
// 或项目特定的包含文件。
#pragma once
#include <string>
#include <map>
#include <thread>
#include <semaphore>
#include <format>
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
	namespace
	{
		inline int fd = 0;
		inline int num = 0;
		int flush_cnt = 0;
		int flush_max = 10;
		std::string file_name;
		void write()
		{
			// write data
			// check if can write
			// if size >= max_size switch to next file
			

		}
		void initFileName()
		{
			// init file_name
			auto& config = Config::get();
			file_name = std::format("{}/{}_{}_{}",config.log_path,config.log_name, std::this_thread::get_id(), num);
		}
	}
};

namespace Logger
{
	namespace // worker
	{
		inline void SyncWork()
		{

		}
		inline void AsyncWork()
		{
			// may be dangerous for mem

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
							SyncWork();
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
		static LogConfig & get()
		{
			static Config config(json);
			return config.__config;
		}
	};
	inline void init()
	{
		// load config file
		// init json & config
		json.read(config_path);
		Config::get();
		// creat worker thread
		initWorker();
	}

	inline bool log(const Msg& msg)
	{
		// push to buffer
		auto& config = Config::get();
		auto& buffer = config.buffer;
		bool ret = buffer.push(msg);
		// awake worker
		awakeWorker();
		return ret;
	}
}
// TODO: 在此处引用程序需要的其他标头。