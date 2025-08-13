set(SOURCES
    vsgaxes.cpp
)

add_executable(vsgaxes ${SOURCES})
set_target_properties(vsgaxes PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgaxes vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgaxes PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgaxes vsgXchange::vsgXchange)
endif()

install(TARGETS vsgaxes RUNTIME DESTINATION bin)
