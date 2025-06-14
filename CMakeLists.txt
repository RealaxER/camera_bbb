cmake_minimum_required(VERSION 3.10)

project(camera_stream)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories(${CMAKE_SOURCE_DIR}/proto)
include_directories(${CMAKE_SOURCE_DIR}/transport/mqtt)
include_directories(${CMAKE_SOURCE_DIR}/transport/p2p)
include_directories(${CMAKE_SOURCE_DIR}/stream)
include_directories(${CMAKE_SOURCE_DIR}/stream/camera)
include_directories(${CMAKE_SOURCE_DIR}/stream/live)
include_directories(${CMAKE_SOURCE_DIR}/stream/record)

set(SHARED_SOURCES
    transport/mqtt/mqtt.cpp
    transport/p2p/p2p.cpp
    stream/baseStream.cpp
    stream/camera/cameraStream.cpp
    stream/live/liveStream.cpp
    stream/record/reocordStream.cpp
    proto/typedef.pb.cc
)

add_executable(camera_stream
    main.cpp
    ${SHARED_SOURCES}
)

add_executable(app
    app.cpp
    ${SHARED_SOURCES}
)

foreach(target IN ITEMS camera_stream app)
    target_link_libraries(${target}
        avformat
        avcodec
        avfilter
        avdevice
        swscale
        avutil
        glog
        pthread
        protobuf
        datachannel
        mosquitto
    )
endforeach()
