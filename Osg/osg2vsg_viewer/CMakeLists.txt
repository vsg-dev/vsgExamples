find_package(OpenGL)

if(NOT ANDROID)
    find_package(Threads)
endif()

if (UNIX)
    find_library(DL_LIBRARY dl)
endif()

set(SOURCES
    Trackball.cpp
    GraphicsNodes.cpp
    osg2vsg_viewer.cpp)

add_executable(osg2vsg_viewer ${SOURCES})
target_include_directories(osg2vsg_viewer PRIVATE ${OSG_INCLUDE_DIR})
target_link_libraries(osg2vsg_viewer vsg::vsg ${OSGDB_LIBRARIES} ${OSGUTIL_LIBRARIES} ${OSG_LIBRARIES} ${OPENTHREADS_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} OpenGL::OpenGL ${DL_LIBRARY})

