set(SOURCES
    vsgperformance.cpp
)

add_executable(vsgperformance ${SOURCES})
set_target_properties(vsgperformance PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgperformance vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgperformance PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgperformance vsgXchange::vsgXchange)
endif()

install(TARGETS vsgperformance RUNTIME DESTINATION bin)
