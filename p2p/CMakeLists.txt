cmake_minimum_required(VERSION 3.12)

set(PACKAGE_MANAGER "hunter")
include("cmake/Hunter/init.cmake")
project(p2p_example)
include("cmake/dependencies.cmake")


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TESTING "Build tests" ON)
option(EXAMPLES "Build examples" ON)
option(CLANG_FORMAT "Enable clang-format target" ON)
option(CLANG_TIDY "Enable clang-tidy checks during compilation" OFF)
option(COVERAGE "Enable generation of coverage info" OFF)
# sanitizers will be enabled only for libp2p, and will be disabled for dependencies
option(ASAN "Enable address sanitizer" OFF)
option(LSAN "Enable leak sanitizer" OFF)
option(MSAN "Enable memory sanitizer" OFF)
option(TSAN "Enable thread sanitizer" OFF)
option(UBSAN "Enable UB sanitizer" OFF)
option(EXPOSE_MOCKS "Make mocks header files visible for child projects" ON)
option(METRICS_ENABLED "Enable libp2p metrics" OFF)
option(SQLITE_ENABLED "Enable sqlite based libp2p storage" OFF)

hunter_add_package(Boost)
find_package(Boost CONFIG REQUIRED)

# look for libp2p in OS
find_package(libp2p CONFIG REQUIRED)


add_executable(main main.cpp)
target_link_libraries(main
    p2p::p2p_multiaddress
    )