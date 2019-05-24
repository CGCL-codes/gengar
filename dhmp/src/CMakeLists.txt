# 查找当前目录下的所有源文件
# 并将名称保存到 DHMP_LIB_SRCS 变量
aux_source_directory(. DHMP_LIB_SRCS)

# set (CMAKE_CXX_FLAGS "-Wall -lxml2 -lrdmacm -libverbs -lpthread -lnuma -lrt -lm")

# MESSAGE(STATUS "THIS " ${DHMP_BINARY_DIR})
# MESSAGE(STATUS "THIS " ${DHMP_SOURCE_DIR})

# 指定生成 MathFunctions 链接库
add_library (dhmp ${DHMP_LIB_SRCS})

SET(LIBRARY_OUTPUT_PATH ${DHMP_SOURCE_DIR}/lib)