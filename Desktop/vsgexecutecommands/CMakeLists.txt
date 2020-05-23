set(SOURCES vsgexecutecommands.cpp)

add_executable(vsgexecutecommands ${SOURCES})

target_link_libraries(vsgexecutecommands vsg::vsg)
