set(SOURCES
    vsgdynamicwindows.cpp
)

add_executable(vsgdynamicwindows ${SOURCES})
set_target_properties(vsgdynamicwindows PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgdynamicwindows vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgdynamicwindows PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgdynamicwindows vsgXchange::vsgXchange)
endif()

install(TARGETS vsgdynamicwindows RUNTIME DESTINATION bin)
