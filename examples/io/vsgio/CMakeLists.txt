set(SOURCES vsgio.cpp)

add_executable(vsgio ${SOURCES})

target_link_libraries(vsgio vsg::vsg)

install(TARGETS vsgio RUNTIME DESTINATION bin)
