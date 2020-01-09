set(SOURCES
    ../vsgviewer/AnimationPath.cpp
    vsgexp.cpp
)

add_executable(vsgexp ${SOURCES})

target_link_libraries(vsgexp vsg::vsg)
