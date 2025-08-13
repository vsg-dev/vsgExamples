set(SOURCES vsgio.cpp)

add_executable(vsgio ${SOURCES})
set_target_properties(vsgio PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgio vsg::vsg)

install(TARGETS vsgio RUNTIME DESTINATION bin)
