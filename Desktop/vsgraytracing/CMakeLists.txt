set(SOURCES
    ../vsgexp/ExperimentalViewer.cpp
    vsgraytracing.cpp
)

add_executable(vsgraytracing ${SOURCES})

target_link_libraries(vsgraytracing vsg::vsg)
