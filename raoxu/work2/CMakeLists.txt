PROJECT(player)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

#将当前目录中的源文件名称赋值给变量 DIR_SRCS
AUX_SOURCE_DIRECTORY(. DIR_SRCS)

#指示变量 DIR_SRCS 中的源文件需要编译 成一个名称为 main 的可执行文件
ADD_EXECUTABLE(player ${DIR_SRCS})

SET(CMAKE_BUILD_TYPE "Debug")  
SET(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -O0 -Wall -g -ggdb")
SET(CMAKE_C_FLAGS_RELEASE "$ENV{CFLAGS} -O3 -Wall") 
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE=1 -D_REENTRANT")

#添加库文件目录相当于 -L
LINK_DIRECTORIES("${PROJECT_SOURCE_DIR}/lib")
LINK_DIRECTORIES("/usr/lib/x86_64-linux-gnu")

#添加头文件的目录相当于 -I
INCLUDE_DIRECTORIES(./ ./include /usr/include/SDL)

#链接动态库相当于-l
TARGET_LINK_LIBRARIES(player avcodec avformat avutil swresample swscale SDL)

