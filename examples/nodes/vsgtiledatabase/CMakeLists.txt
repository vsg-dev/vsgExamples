set(SOURCES
    vsgtiledatabase.cpp
)

add_executable(vsgtiledatabase ${SOURCES})

target_link_libraries(vsgtiledatabase )

target_link_libraries(vsgtiledatabase vsg::vsg vsgXchange::vsgXchange)

install(TARGETS vsgtiledatabase RUNTIME DESTINATION bin)
