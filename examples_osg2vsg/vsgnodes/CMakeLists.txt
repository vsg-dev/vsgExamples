set(SOURCES vsgnodes.cpp)

add_executable(vsgnodes ${SOURCES})

target_link_libraries(vsgnodes vsg::vsg osg2vsg::osg2vsg)
