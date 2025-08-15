cmake_minimum_required(VERSION 3.7)

project(vsgExamples
        VERSION 1.1.9
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

find_package(vsg 1.1.11)

vsg_setup_dir_vars()
vsg_setup_build_vars()

# find the optional vsgXchange that can be used for reading a range of image and 3d model formats and shader compilation
find_package(vsgXchange 1.1.7 QUIET)

# find the optional vsgImGui that can be used for GUI elements added into graphics windows.
find_package(vsgImGui QUIET)

find_package(Tracy QUIET)

# set the use of C++17 globally as all examples require it
set(CMAKE_CXX_STANDARD 17)

if (MSVC)
    # MSVC assumes system-wide eight-bit code page (e.g. CP1252, CP1250) if there's no BOM
    add_compile_options("/utf-8")
endif()

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

# Recursively get all the targets created in current_dir; store the list in out_var.
# https://discourse.cmake.org/t/cmake-list-of-all-project-targets/1077/17
function (_get_all_cmake_targets out_var current_dir)
    get_property(targets DIRECTORY ${current_dir} PROPERTY BUILDSYSTEM_TARGETS)
    get_property(subdirs DIRECTORY ${current_dir} PROPERTY SUBDIRECTORIES)

    foreach(subdir ${subdirs})
        _get_all_cmake_targets(subdir_targets ${subdir})
        list(APPEND targets ${subdir_targets})
    endforeach()

    set(${out_var} ${targets} PARENT_SCOPE)
endfunction()

# VSG examples
_get_all_cmake_targets(all_targets_pre_examples ${CMAKE_CURRENT_LIST_DIR})
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
_get_all_cmake_targets(all_targets_post_examples ${CMAKE_CURRENT_LIST_DIR})

# Get and store into the global property vsgExamples_all_examples_targets all the example targets that were created.
set(all_targets_post_examples_copy ${all_targets_post_examples})
foreach(target IN LISTS all_targets_post_examples_copy)
    list(FIND all_targets_pre_examples ${target} index)
    if (index GREATER -1)
        list(REMOVE_ITEM all_targets_post_examples ${target})
    endif()
endforeach()
set_property(GLOBAL PROPERTY vsgExamples_all_examples_targets "${all_targets_post_examples}")

# VSG tests
_get_all_cmake_targets(all_targets_pre_tests ${CMAKE_CURRENT_LIST_DIR})
add_subdirectory(tests)
_get_all_cmake_targets(all_targets_post_test ${CMAKE_CURRENT_LIST_DIR})

# Get and store into the global property vsgExamples_all_tests_targets all the test targets that were created.
set(all_targets_post_test_copy ${all_targets_post_test})
foreach(target IN LISTS all_targets_post_test_copy)
    list(FIND all_targets_pre_tests ${target} index)
    if (index GREATER -1)
        list(REMOVE_ITEM all_targets_post_test ${target})
    endif()
endforeach()
set_property(GLOBAL PROPERTY vsgExamples_all_tests_targets "${all_targets_post_test}")

vsg_add_feature_summary()
