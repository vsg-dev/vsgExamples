set(SOURCES
    vsgstoragebuffer.cpp
)

add_executable(vsgstoragebuffer ${SOURCES})

target_link_libraries(vsgstoragebuffer vsg::vsg)

install(TARGETS vsgstoragebuffer RUNTIME DESTINATION bin)
