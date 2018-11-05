set(SOURCES vsgdraw.cpp)

add_executable(vsgdraw ${SOURCES})

target_link_libraries(vsgdraw vsg::vsg osg2vsg::osg2vsg)
