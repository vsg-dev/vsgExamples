set(SOURCES vsgtypes.cpp)

add_executable(vsgtypes ${SOURCES})
set_target_properties(vsgtypes PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgtypes vsg::vsg)

install(TARGETS vsgtypes RUNTIME DESTINATION bin)
