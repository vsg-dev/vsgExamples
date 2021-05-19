set(SOURCES vsgtypes.cpp)

add_executable(vsgtypes ${SOURCES})

target_link_libraries(vsgtypes vsg::vsg)

install(TARGETS vsgtypes RUNTIME DESTINATION bin)
