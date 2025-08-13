set(SOURCES
    vsgheadless.cpp
)

add_executable(vsgheadless ${SOURCES})
set_target_properties(vsgheadless PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgheadless vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgheadless PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgheadless vsgXchange::vsgXchange)
endif()

install(TARGETS vsgheadless RUNTIME DESTINATION bin)
