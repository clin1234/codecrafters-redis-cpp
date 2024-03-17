cmake_minimum_required(VERSION 3.13)

project(redis-starter-cpp)
set(CMAKE_CXX_STANDARD 20) # Enable the C++20 standard

set(SOURCE_FILES src/Server.cpp)

find_package(Threads)

if (CMAKE_USE_PTHREADS_INIT)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	target_compile_options(server PUBLIC "-pthread")
	target_link_libraries(server pthread)
endif()

add_executable(server ${SOURCE_FILES})