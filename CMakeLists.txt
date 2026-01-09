cmake_minimum_required(VERSION 3.10)
project(ThreadPoolProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含目录
include_directories(
    cachedthreadpool/include
    fixedthreadpool/include
    scheduledthreadpool/include
    singlethreadpool/include
    workstealthreadpool/include
)

# 添加可执行文件
add_executable(test test/test.cpp)