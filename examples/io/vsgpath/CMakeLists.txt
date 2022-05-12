set(SOURCES vsgpath.cpp)

add_executable(vsgpath ${SOURCES})

target_link_libraries(vsgpath vsg::vsg)

install(TARGETS vsgpath RUNTIME DESTINATION bin)
