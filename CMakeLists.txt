cmake_minimum_required(VERSION 3.5)

project(vsgFramework)

# find the various depdendencies
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_SOURCE_DIR}/CMakeModules/")
include(GitExternal)
include(ExternalProject)

set(OUTPUT_BINDIR ${PROJECT_BINARY_DIR}/bin)
set(OUTPUT_LIBDIR ${PROJECT_BINARY_DIR}/lib)
set(COMPONENT_DIR ${PROJECT_BINARY_DIR}/components)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})
if(WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})
else(WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
endif(WIN32)

# set up local bin directory to place all binaries
if (NOT EXISTS ${OUTPUT_BINDIR})
    make_directory(${OUTPUT_BINDIR})
endif()

if (NOT EXISTS ${OUTPUT_LIBDIR})
    make_directory(${OUTPUT_LIBDIR})
endif()

if (NOT EXISTS ${COMPONENT_DIR})
#    make_directory(${COMPONENT_DIR})
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})

# set compilation to use C++11
set(CMAKE_CXX_STANDARD 11)


# Dynamic vs Static Linking
option(VSGFRAMEWORK_SHARED_LIBRARY "Set to ON to build libraries for dynamic linking.  Use OFF for static." ON)


find_package(GLFW)
find_package(Vulkan)
find_package(OpenSceneGraph)
find_package(VSG)
find_package(osg2vsg)

find_package(Threads)

find_library(DL_LIBRARY dl)
if(NOT DL_LIBRARY)
    set(DL_LIBRARY "") # change from NOTFOUND to empty when passed to linker
endif()



if (NOT VSG_FOUND)
    ExternalProject_Add(
        VSG

        PREFIX components

        GIT_REPOSITORY "https://github.com/robertosfield/VulkanSceneGraphPrototype.git"
        GIT_TAG "master"

        UPDATE_COMMAND ""
        PATCH_COMMAND ""

        SOURCE_DIR "${COMPONENT_DIR}/VulkanSceneGraphPrototype"
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}
    )
endif()

if (NOT OSG2VSG_FOUND)
    ExternalProject_Add(
        OSG2VSG

        PREFIX components

        GIT_REPOSITORY "https://github.com/robertosfield/osg2vsg.git"
        GIT_TAG "master"

        UPDATE_COMMAND ""
        PATCH_COMMAND ""

        SOURCE_DIR "${COMPONENT_DIR}/osg2vsg"
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR} -DVSG_DIR=${PROJECT_BINARY_DIR}
    )
endif()
