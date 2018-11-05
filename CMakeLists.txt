cmake_minimum_required(VERSION 3.7)

project(vsgExamples
        VERSION 0.0.0
        DESCRIPTION "Set of example programs that test and illustrate how to use the Vulkan/VkSceneGraph"
        LANGUAGES CXX C
)

# build all examples into the bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# find the vsg
find_package(vsg REQUIRED)

# Find osg2vsg
find_package(osg2vsg)

# set the use of C++17 globally as all examples require it
set(CMAKE_CXX_STANDARD 17)

# add clobber build target to clear all the non git registered files/directories
add_custom_target(clobber
    COMMAND git clean -d -f -x
)

# pure VSG examples
add_subdirectory(examples_vsg)

if (osg2vsg_FOUND)
    # examples that required VSG and osg2vsg (and hence OpenSceneGraph)
    add_subdirectory(examples_osg2vsg)
endif()
