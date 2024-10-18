set(SOURCES
    vsgcompilemanager.cpp
)

add_executable(vsgcompilemanager ${SOURCES})

target_link_libraries(vsgcompilemanager vsg::vsg)

install(TARGETS vsgcompilemanager RUNTIME DESTINATION bin)
