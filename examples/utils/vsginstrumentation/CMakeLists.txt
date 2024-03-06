set(SOURCES
    vsginstrumentation.cpp
)

add_executable(vsginstrumentation ${SOURCES})

target_link_libraries(vsginstrumentation vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsginstrumentation PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsginstrumentation vsgXchange::vsgXchange)
endif()

install(TARGETS vsginstrumentation RUNTIME DESTINATION bin)
