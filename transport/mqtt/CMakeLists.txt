cmake_minimum_required(VERSION 3.0)
project(test)


include_directories(${CMAKE_CURRENT_SOURCE_DIR})


set(SOURCES 
    main.cpp
    mqtt.cpp
)

add_executable(test ${SOURCES})

target_link_libraries(test mosquitto pthread)