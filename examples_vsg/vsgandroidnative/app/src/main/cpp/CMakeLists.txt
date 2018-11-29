cmake_minimum_required(VERSION 3.13.0)

# build native_app_glue as a static lib
set(${CMAKE_C_FLAGS}, "${CMAKE_C_FLAGS}")
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

# now build app's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

# set the use of C++17 globally as all examples require it
set(CMAKE_CXX_STANDARD 17)

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_library(native-activity SHARED main.cpp)

target_include_directories(native-activity PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue)

# find the vsg
find_package(vsg REQUIRED)

# add lib dependencies
target_link_libraries(native-activity
    android
    native_app_glue
    EGL
    GLESv1_CM
    log
    vsg::vsg)
