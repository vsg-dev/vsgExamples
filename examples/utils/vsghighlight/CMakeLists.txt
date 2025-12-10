set(SOURCES
    vsghighlight.cpp
)

add_executable(vsghighlight ${SOURCES})

target_link_libraries(vsghighlight vsg::vsg)

install(TARGETS vsghighlight RUNTIME DESTINATION bin)
