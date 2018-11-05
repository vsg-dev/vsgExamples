set(SOURCES vsgintrospection.cpp)

add_executable(vsgintrospection ${SOURCES})

target_link_libraries(vsgintrospection vsg::vsg)
