set(SOURCES vsgreallocations.cpp)

add_executable(vsgreallocations ${HEADERS} ${SOURCES})
set_target_properties(vsgreallocations PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgreallocations vsg::vsg)

install(TARGETS vsgreallocations RUNTIME DESTINATION bin)
