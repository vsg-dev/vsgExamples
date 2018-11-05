set(SOURCES vsgpushconstants.cpp)

add_executable(vsgpushconstants ${SOURCES})

target_link_libraries(vsgpushconstants vsg::vsg osg2vsg::osg2vsg)
