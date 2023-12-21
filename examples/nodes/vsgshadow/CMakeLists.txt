set(SOURCES
    vsgshadow.cpp
)

add_executable(vsgshadow ${SOURCES})

target_link_libraries(vsgshadow vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgshadow PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgshadow vsgXchange::vsgXchange)
endif()

if (Tracy_FOUND)
    target_compile_definitions(vsgshadow PRIVATE Tracy_FOUND)
    target_link_libraries(vsgshadow Tracy::TracyClient)
endif()

install(TARGETS vsgshadow RUNTIME DESTINATION bin)
