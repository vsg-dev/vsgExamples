set(SOURCES
    vsgperformance.cpp
)

add_executable(vsgperformance ${SOURCES})

target_link_libraries(vsgperformance vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgperformance PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgperformance vsgXchange::vsgXchange)
endif()

install(TARGETS vsgperformance RUNTIME DESTINATION bin)
