set(SOURCES
    vsgtextgroup.cpp
)

add_executable(vsgtextgroup ${SOURCES})
set_target_properties(vsgtextgroup PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgtextgroup vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgtextgroup PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgtextgroup vsgXchange::vsgXchange)
endif()

install(TARGETS vsgtextgroup RUNTIME DESTINATION bin)
