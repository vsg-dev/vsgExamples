set(SOURCES vsgcompute.cpp)

add_executable(vsgcompute ${SOURCES})
set_target_properties(vsgcompute PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgcompute vsg::vsg)

install(TARGETS vsgcompute RUNTIME DESTINATION bin)
