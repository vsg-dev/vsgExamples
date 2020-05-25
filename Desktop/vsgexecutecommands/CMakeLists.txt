set(SOURCES
    ../vsgviewer/AnimationPath.cpp
    vsgexecutecommands.cpp
)

add_executable(vsgexecutecommands ${SOURCES})

target_include_directories(vsgexecutecommands PRIVATE ../vsgviewer)

target_link_libraries(vsgexecutecommands vsg::vsg)
