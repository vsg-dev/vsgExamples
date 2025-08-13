set(SOURCES vsginterleaved.cpp)

add_executable(vsginterleaved ${SOURCES})
set_target_properties(vsginterleaved PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsginterleaved vsg::vsg)

install(TARGETS vsginterleaved RUNTIME DESTINATION bin)
