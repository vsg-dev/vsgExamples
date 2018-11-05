set(SOURCES vsgnamespaces.cpp)

add_executable(vsgnamespaces ${SOURCES})

target_link_libraries(vsgnamespaces vsg::vsg)
