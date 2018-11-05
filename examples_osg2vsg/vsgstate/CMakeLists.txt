set(SOURCES vsgstate.cpp)

add_executable(vsgstate ${SOURCES})

target_link_libraries(vsgstate vsg::vsg osg2vsg::osg2vsg)
