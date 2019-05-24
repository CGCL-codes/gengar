# CMake 最低版本号要求
cmake_minimum_required (VERSION 2.6)

# 项目信息
project (DHMP)

# 添加 math 子目录
include_directories(include)
include_directories(/usr/local/include/libxml2/)
include_directories(/home/rdma_match/xinzh/include)
link_directories(/home/rdma_match/xinzh/lib)

add_subdirectory(src)

add_subdirectory(test)