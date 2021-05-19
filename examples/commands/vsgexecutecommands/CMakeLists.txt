set(SOURCES
    ../../shared/AnimationPath.cpp
    vsgexecutecommands.cpp
)

add_executable(vsgexecutecommands ${SOURCES})

target_link_libraries(vsgexecutecommands vsg::vsg)

install(TARGETS vsgexecutecommands RUNTIME DESTINATION bin)
