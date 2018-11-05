set(SOURCES vsgobjects.cpp)

add_executable(vsgobjects ${SOURCES})

target_link_libraries(vsgobjects vsg::vsg osg2vsg::osg2vsg)
