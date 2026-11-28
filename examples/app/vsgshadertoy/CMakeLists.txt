set(SOURCES
    vsgshadertoy.cpp
)

add_executable(vsgshadertoy ${SOURCES})

target_link_libraries(vsgshadertoy vsg::vsg)

install(TARGETS vsgshadertoy RUNTIME DESTINATION bin)
