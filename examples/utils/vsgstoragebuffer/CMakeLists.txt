set(SOURCES
    vsgstoragebuffer.cpp
)

add_executable(vsgstoragebuffer ${SOURCES})
set_target_properties(vsgstoragebuffer PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgstoragebuffer vsg::vsg)

install(TARGETS vsgstoragebuffer RUNTIME DESTINATION bin)
