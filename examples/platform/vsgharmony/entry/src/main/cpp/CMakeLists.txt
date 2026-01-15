# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(VsgHarmony)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()
#set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "D:/cmakelib_harmony/armv8a")
find_package(vsg REQUIRED NO_CMAKE_FIND_ROOT_PATH)

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)

add_library(entry SHARED napi_init.cpp)
target_link_libraries(entry PUBLIC libace_napi.z.so
                vsg::vsg
                hilog_ndk.z #log
                ace_ndk.z
                native_window
                )