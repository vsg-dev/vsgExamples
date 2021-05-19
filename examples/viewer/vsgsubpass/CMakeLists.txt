set(SOURCES vsgsubpass.cpp)

add_executable(vsgsubpass ${SOURCES})

target_link_libraries(vsgsubpass vsg::vsg)

install(TARGETS vsgsubpass RUNTIME DESTINATION bin)
