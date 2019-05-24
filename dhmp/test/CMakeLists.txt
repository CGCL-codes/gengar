# 查找目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量
# aux_source_directory(. DIR_SRCS)
set (CLIENT_SRCS client.c)
set (SERVER_SRCS server.c)
set (WATCHER_SRCS watcher.c)

#set (CMAKE_CXX_FLAGS "-Wall -lxml2 -lrdmacm -libverbs -lpthread -lnuma -lrt -lm")

set (EXECUTABLE_OUTPUT_PATH ${DHMP_SOURCE_DIR}/bin)

# 指定生成目标
add_executable(client ${CLIENT_SRCS})
add_executable(server ${SERVER_SRCS})
add_executable(watcher ${WATCHER_SRCS})

# 添加链接库
target_link_libraries(client dhmp)
target_link_libraries(client xml2)
target_link_libraries(client pthread)
target_link_libraries(client rdmacm)
target_link_libraries(client ibverbs)
target_link_libraries(client numa)
target_link_libraries(client rt)
target_link_libraries(client m)
target_link_libraries(client json-c)

target_link_libraries(server dhmp)
target_link_libraries(server xml2)
target_link_libraries(server pthread)
target_link_libraries(server rdmacm)
target_link_libraries(server ibverbs)
target_link_libraries(server numa)
target_link_libraries(server rt)
target_link_libraries(server m)
target_link_libraries(server json-c)

target_link_libraries(watcher dhmp)
target_link_libraries(watcher xml2)
target_link_libraries(watcher pthread)
target_link_libraries(watcher rdmacm)
target_link_libraries(watcher ibverbs)
target_link_libraries(watcher numa)
target_link_libraries(watcher rt)
target_link_libraries(watcher m)
target_link_libraries(watcher json-c)