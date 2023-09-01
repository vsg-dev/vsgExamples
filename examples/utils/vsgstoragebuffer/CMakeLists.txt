set(SOURCES
    vsgstoragebuffer.cpp
)

add_executable(vsgstoragebuffer ${SOURCES})

target_link_libraries(vsgstoragebuffer vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgstoragebuffer PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgstoragebuffer vsgXchange::vsgXchange)
endif()

install(TARGETS vsgstoragebuffer RUNTIME DESTINATION bin)
