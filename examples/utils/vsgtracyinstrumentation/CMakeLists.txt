set(SOURCES
    vsgtracyinstrumentation.cpp
)

add_executable(vsgtracyinstrumentation ${SOURCES})
set_target_properties(vsgtracyinstrumentation PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgtracyinstrumentation vsg::vsg Tracy::TracyClient)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgtracyinstrumentation PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgtracyinstrumentation vsgXchange::vsgXchange)
endif()

install(TARGETS vsgtracyinstrumentation RUNTIME DESTINATION bin)
