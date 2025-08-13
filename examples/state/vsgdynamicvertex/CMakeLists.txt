set(SOURCES
    vsgdynamicvertex.cpp
)

add_executable(vsgdynamicvertex ${SOURCES})
set_target_properties(vsgdynamicvertex PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgdynamicvertex vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgdynamicvertex PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgdynamicvertex vsgXchange::vsgXchange)
endif()

install(TARGETS vsgdynamicvertex RUNTIME DESTINATION bin)
