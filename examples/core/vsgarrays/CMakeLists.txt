set(SOURCES vsgarrays.cpp)

add_executable(vsgarrays ${SOURCES})
set_target_properties(vsgarrays PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgarrays vsg::vsg)

install(TARGETS vsgarrays RUNTIME DESTINATION bin)
