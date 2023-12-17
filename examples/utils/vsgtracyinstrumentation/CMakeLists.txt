set(SOURCES
    vsgtracyinstrumentation.cpp
)

add_executable(vsgtracyinstrumentation ${SOURCES})

target_link_libraries(vsgtracyinstrumentation vsg::vsg Tracy::TracyClient)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgtracyinstrumentation PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgtracyinstrumentation vsgXchange::vsgXchange)
endif()

install(TARGETS vsgtracyinstrumentation RUNTIME DESTINATION bin)
