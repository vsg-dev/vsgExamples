set(SOURCES vsgsubpass.cpp)

add_executable(vsgsubpass ${SOURCES})
set_target_properties(vsgsubpass PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgsubpass vsg::vsg)

install(TARGETS vsgsubpass RUNTIME DESTINATION bin)
