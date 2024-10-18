set(SOURCES vsgreallocations.cpp)

add_executable(vsgreallocations ${HEADERS} ${SOURCES})
target_link_libraries(vsgreallocations vsg::vsg)

install(TARGETS vsgreallocations RUNTIME DESTINATION bin)
