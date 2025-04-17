cmake_minimum_required(VERSION 3.10)
project(datachannel-jni)

set(CMAKE_CXX_STANDARD 17)

add_library(
    datachannel-jni
    SHARED
    cpp/p2p_jni.cpp
    cpp/p2p.cpp
)

target_include_directories(
    datachannel-jni
    PRIVATE
    cpp
    ${LIBDATACHANNEL_INCLUDE_PATH}
)

find_library(log-lib log)

target_link_libraries(
    datachannel-jni
    ${log-lib}
    ${LIBDATACHANNEL_PATH}/libdatachannel.so
)

add_custom_command(TARGET datachannel-jni POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${LIBDATACHANNEL_PATH}/libdatachannel.so
        ${CMAKE_BINARY_DIR}/libdatachannel.so
)

add_custom_command(TARGET datachannel-jni POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${LIBDATACHANNEL_PATH}/libssl.so
        ${CMAKE_BINARY_DIR}/libssl.so
)

add_custom_command(TARGET datachannel-jni POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${LIBDATACHANNEL_PATH}/libcrypto.so
        ${CMAKE_BINARY_DIR}/libcrypto.so
)
