cmake_minimum_required(VERSION 3.7)

project(vsgExamples
        VERSION 1.1.8
        DESCRIPTION "Set of example programs that test and illustrate how to use the VulkanSceneGraph"
        LANGUAGES CXX C
)

SET(VSGEXAMPLES_RELEASE_CANDIDATE 0)

# build all examples into the bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# Change the default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# find the vsg
if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

find_package(vsg 1.1.10)

vsg_setup_dir_vars()
vsg_setup_build_vars()

# find the optional vsgXchange that can be used for reading a range of image and 3d model formats and shader compilation
find_package(vsgXchange 1.1.6 QUIET)

# find the optional vsgImGui that can be used for GUI elements added into graphics windows.
find_package(vsgImGui QUIET)

find_package(Tracy QUIET)

# set the use of C++17 globally as all examples require it
set(CMAKE_CXX_STANDARD 17)

vsg_add_target_clang_format(
    FILES
        ${CMAKE_SOURCE_DIR}/*/*/*.h
        ${CMAKE_SOURCE_DIR}/*/*/*.cpp
        ${CMAKE_SOURCE_DIR}/*/*/*/*.h
        ${CMAKE_SOURCE_DIR}/*/*/*/*.cpp
)
vsg_add_target_clobber()
vsg_add_target_cppcheck(
    FILES
        examples/
)
vsg_add_target_docs(
    FILES
        examples/
)
vsg_add_target_uninstall()

# only provide custom targets if not building as a submodule/FetchContent
if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})

    vsg_add_option_maintainer(
        PREFIX v
        RCLEVEL ${VSGEXAMPLES_RELEASE_CANDIDATE}
    )

endif()

# install data
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/ DESTINATION share/vsgExamples)

# VSG examples
add_subdirectory(examples/animation)
add_subdirectory(examples/app)
add_subdirectory(examples/commands)
add_subdirectory(examples/core)
add_subdirectory(examples/io)
add_subdirectory(examples/lighting)
add_subdirectory(examples/maths)
add_subdirectory(examples/meshshaders)
add_subdirectory(examples/nodes)
add_subdirectory(examples/platform)
add_subdirectory(examples/raytracing)
add_subdirectory(examples/state)
add_subdirectory(examples/text)
add_subdirectory(examples/threading)
add_subdirectory(examples/ui)
add_subdirectory(examples/utils)
add_subdirectory(examples/vk)
add_subdirectory(examples/volume)

# VSG tests
add_subdirectory(tests)

vsg_add_feature_summary()
