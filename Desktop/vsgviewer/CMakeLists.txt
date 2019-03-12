set(SOURCES
    vsgviewer.cpp
)

add_executable(vsgviewer ${SOURCES})

target_link_libraries(vsgviewer vsg::vsg)
