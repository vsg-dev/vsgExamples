set(SOURCES
    vsglod.cpp
)

add_executable(vsglod ${SOURCES})

target_link_libraries(vsglod vsg::vsg)

if (Tracy_FOUND)
    target_compile_definitions(vsglod PRIVATE Tracy_FOUND)
    target_link_libraries(vsglod Tracy::TracyClient)
endif()

install(TARGETS vsglod RUNTIME DESTINATION bin)
