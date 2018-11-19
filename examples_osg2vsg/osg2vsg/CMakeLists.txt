set(SOURCES osg2vsg.cpp)

add_executable(osg2vsg ${SOURCES})

target_link_libraries(osg2vsg vsg::vsg osg2vsg::osg2vsg)
