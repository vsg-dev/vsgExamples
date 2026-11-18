cmake_minimum_required(VERSION 3.7)

project(shadowacne)

set(CMAKE_CXX_STANDARD 17)

find_package(vsg REQUIRED)

add_executable(shadowacne shadowacne.cpp)
target_link_libraries(shadowacne vsg::vsg)

# --screenshot support (capture.h) for producing the comparison images
option(SHADOWACNE_CAPTURE "Enable --frames/--screenshot" OFF)
if(SHADOWACNE_CAPTURE)
    target_compile_definitions(shadowacne PRIVATE SHADOWACNE_CAPTURE)
endif()
