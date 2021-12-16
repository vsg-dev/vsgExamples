cmake_minimum_required(VERSION 3.7)

project(vsgExamples
        VERSION 0.0.0
        DESCRIPTION "Set of example programs that test and illustrate how to use the VulkanSceneGraph"
        LANGUAGES CXX C
)

SET(vsgExamples_RELEASE_CANDIDATE 0)

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

find_package(vsg 0.2.2 REQUIRED)

# find the optional vsgExamples that can be used for reading and range of image and 3d model formats and shader compilation
find_package(vsgXchange QUIET)

# find the optional vsgImGui that can be used for GUI elements added into graphics window.
find_package(vsgImGui QUIET)

# set the use of C++17 globally as all examples require it
set(CMAKE_CXX_STANDARD 17)

vsg_setup_build_vars()
vsg_setup_dir_vars()

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

vsg_add_option_maintainer(
    PREFIX vsgExamples
    RCLEVEL vsgExamples_RELEASE_CANDIDATE
)

# pure VSG examples
add_subdirectory(examples/core)
add_subdirectory(examples/maths)
add_subdirectory(examples/io)
add_subdirectory(examples/ui)
add_subdirectory(examples/state)
add_subdirectory(examples/commands)
add_subdirectory(examples/nodes)
add_subdirectory(examples/traversals)
add_subdirectory(examples/threading)
add_subdirectory(examples/viewer)
add_subdirectory(examples/vk)
add_subdirectory(examples/text)
add_subdirectory(examples/rtx)
add_subdirectory(examples/raytracing)
add_subdirectory(examples/introspection)
add_subdirectory(examples/platform)
add_subdirectory(examples/utils)

vsg_add_feature_summary()
