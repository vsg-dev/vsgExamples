set(SOURCES
    vsgheadless.cpp
)

add_executable(vsgheadless ${SOURCES})

target_link_libraries(vsgheadless vsg::vsg)
