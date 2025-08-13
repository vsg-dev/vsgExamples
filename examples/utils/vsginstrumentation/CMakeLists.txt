set(SOURCES
    vsginstrumentation.cpp
)

add_executable(vsginstrumentation ${SOURCES})
set_target_properties(vsginstrumentation PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsginstrumentation vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsginstrumentation PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsginstrumentation vsgXchange::vsgXchange)
endif()

install(TARGETS vsginstrumentation RUNTIME DESTINATION bin)
