# CMakeList.txt: Logger 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()
project ("Logger")

include_directories("./json")

set(CMAKE_CXX_STANDARD 20)

set(JSON_DIR "json")
file(GLOB_RECURSE JSON_FILES ${JSON_DIR}/*.h ${JSON_DIR}/*.cpp)
find_package(FMT REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g  -std=c++20  -lfmt")

# 将源代码添加到此项目的可执行文件。
add_library(myjson STATIC ${JSON_FILES})
set(CMAKE_CXX_STANDARD 20)

add_executable (Logger "Logger.cpp" "Logger.h" "Time.h" "Buffer.h")

target_link_libraries("Logger" myjson fmt)
