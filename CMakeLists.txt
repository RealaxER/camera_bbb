cmake_minimum_required(VERSION 3.10)

project(camera_stream)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -pthread")

add_executable(stream stream.cpp)

target_link_libraries(stream
    avformat
    avcodec
    avfilter
    avdevice
    swscale
    avutil
    glog
    pthread
)
