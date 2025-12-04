set(SOURCES
    vsgloadbalancing.cpp
)

add_executable(vsgloadbalancing ${SOURCES})

target_link_libraries(vsgloadbalancing vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgloadbalancing PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgloadbalancing vsgXchange::vsgXchange)
endif()

install(TARGETS vsgloadbalancing RUNTIME DESTINATION bin)
