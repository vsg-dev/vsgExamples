set(SOURCES vsgcompute.cpp)

add_executable(vsgcompute ${SOURCES})

target_link_libraries(vsgcompute vsg::vsg osg2vsg::osg2vsg)
