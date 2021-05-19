set(SOURCES vsgarrays.cpp)

add_executable(vsgarrays ${SOURCES})

target_link_libraries(vsgarrays vsg::vsg)

install(TARGETS vsgarrays RUNTIME DESTINATION bin)
