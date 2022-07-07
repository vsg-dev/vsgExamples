set(SOURCES vsgtimestamps.cpp)

add_executable(vsgtimestamps ${SOURCES})

target_link_libraries(vsgtimestamps vsg::vsg)

install(TARGETS vsgtimestamps RUNTIME DESTINATION bin)
