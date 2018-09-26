cmake_minimum_required(VERSION 3.5)

project(vsgFramework)

# find the various depdendencies
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_SOURCE_DIR}/CMakeModules/")
include(ExternalProject)

set(OUTPUT_BINDIR ${PROJECT_BINARY_DIR}/bin)
set(OUTPUT_LIBDIR ${PROJECT_BINARY_DIR}/lib)
set(OUTPUT_INCDIR ${PROJECT_BINARY_DIR}/include)
set(COMPONENT_DIR ${PROJECT_BINARY_DIR}/components)
set(PREFIX_PATH   ${CMAKE_PREFIX_PATH} ${PROJECT_BINARY_DIR})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})
if(WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})
else(WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
endif(WIN32)




set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})

# set compilation to use C++11
set(CMAKE_CXX_STANDARD 17)


# Dynamic vs Static Linking
option(VSGFRAMEWORK_SHARED_LIBRARY "Set to ON to build libraries for dynamic linking.  Use OFF for static." ON)


macro(addExternal TARGET_NAME REPOSITORY TAG EXTRA_CMAKE_ARGS)
    ExternalProject_Add(
        ${TARGET_NAME}

        PREFIX components

        GIT_REPOSITORY ${REPOSITORY}
        GIT_TAG ${TAG}

        UPDATE_COMMAND ""
        PATCH_COMMAND ""

        SOURCE_DIR "${COMPONENT_DIR}/${TARGET_NAME}"
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR} -DCMAKE_PREFIX_PATH=${PREFIX_PATH} ${EXTRA_CMAKE_ARGS}
    )
endmacro()


find_package(GLFW)
find_package(Vulkan)
find_package(Threads)

find_library(DL_LIBRARY dl)
if(NOT DL_LIBRARY)
    set(DL_LIBRARY "") # change from NOTFOUND to empty when passed to linker
endif()


find_package(osg)
find_package(osgDB)
find_package(osgUtil)
if (NOT OSG_FOUND)
    set(TARGET_OSG OpenSceneGraph)
    addExternal(${TARGET_OSG} "https://github.com/openscenegraph/OpenSceneGraph.git" "OpenSceneGraph-3.6" "-DLIB_POSTFIX=")
    set(OSG_INCLUDE_DIR ${OUTPUT_INCDIR} CACHE PATH "OSG header directory" FORCE)
    set(OSG_LIBRARIES ${OUTPUT_LIBDIR}/libosg.so CACHE FILEPATH "osg Library" FORCE)
    set(OSGDB_LIBRARIES ${OUTPUT_LIBDIR}/libosgDB.so CACHE FILEPATH "osgUtl Library" FORCE)
    set(OSGUTIL_LIBRARIES ${OUTPUT_LIBDIR}/libosgUtil.so CACHE FILEPATH "osgDB Library" FORCE)
    set(OSG_FOUND TRUE CACHE BOOL "OSG Library" FORCE)
    message("In OSG Fallback setup")
endif()

find_package(VSG)
if (NOT VSG_FOUND)
    set(TARGET_VSG VulkanSceneGraph)
    addExternal(${TARGET_VSG} "https://github.com/robertosfield/VulkanSceneGraphPrototype.git" "master" "")
    if (TARGET_OSG)
        ExternalProject_Add_StepDependencies(${TARGET_VSG} configure ${TARGET_OSG})
    endif()
    set(VSG_INCLUDE_DIR ${OUTPUT_INCDIR} CACHE PATH "vsg header directory" FORCE)
    set(VSG_LIBRARY ${OUTPUT_LIBDIR}/libvsg.so CACHE FILEPATH "vsg Library" FORCE)
    set(VSG_LIBRARIES ${VSG_LIBRARY})
    set(VSG_FOUND TRUE CACHE BOOL "vsg Library" FORCE)
    message("In VSG Fallback setup")
endif()

find_package(osg2vsg)
if (NOT OSG2VSG_FOUND)
    set(TARGET_OSG2VSG osg2vsg)
    addExternal(${TARGET_OSG2VSG} "https://github.com/robertosfield/osg2vsg.git" "master" "")
    if (TARGET_OSG OR TARGET_VSG)
        ExternalProject_Add_StepDependencies(${TARGET_OSG2VSG} configure ${TARGET_OSG} ${TARGET_VSG})
    endif()
    set(OSG2VSG_INCLUDE_DIR ${OUTPUT_INCDIR} CACHE PATH "osg2vsg header directory" FORCE)
    set(OSG2VSG_LIBRARY ${OUTPUT_LIBDIR}/libosg2vsg.so CACHE FILEPATH "osg2vsg Library" FORCE)
    set(OSG2VSG_LIBRARIES ${OSG2VSG_LIBRARY})
    set(OSG2VSG_FOUND TRUE CACHE BOOL "vsg2osg Library" FORCE)
    message("In OSG2VSG Fallback setup")
endif()

add_custom_target(clobber
    COMMAND git clean -d -f -x
    COMMAND rm -rf components
)

add_subdirectory(applications)
