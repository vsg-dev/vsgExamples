set(SOURCES
    vsgcompilemanager.cpp
)

add_executable(vsgcompilemanager ${SOURCES})
set_target_properties(vsgcompilemanager PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgcompilemanager vsg::vsg)

install(TARGETS vsgcompilemanager RUNTIME DESTINATION bin)
