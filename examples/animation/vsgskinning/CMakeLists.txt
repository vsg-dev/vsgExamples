set(SOURCES
    vsgskinning.cpp
)

add_executable(vsgskinning ${SOURCES})

target_link_libraries(vsgskinning vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgskinning PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgskinning vsgXchange::vsgXchange)
endif()

install(TARGETS vsgskinning RUNTIME DESTINATION bin)
