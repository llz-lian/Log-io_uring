// Logger.cpp: 定义应用程序的入口点。
//

#include "Logger.h"

using namespace std;



int main()
{
	Logger::init();
	auto & ret = Logger::Config::get();
	return 0;
}
