set(SOURCES
    vsgtiledatabase.cpp
)

add_executable(vsgtiledatabase ${SOURCES})
set_target_properties(vsgtiledatabase PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgtiledatabase )

target_link_libraries(vsgtiledatabase vsg::vsg vsgXchange::vsgXchange)

install(TARGETS vsgtiledatabase RUNTIME DESTINATION bin)
