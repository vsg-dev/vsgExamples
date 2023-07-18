set(SOURCES
    vsgshadows.cpp
)

add_executable(vsgshadows ${SOURCES})

target_link_libraries(vsgshadows vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgshadows PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgshadows vsgXchange::vsgXchange)
endif()

install(TARGETS vsgshadows RUNTIME DESTINATION bin)
