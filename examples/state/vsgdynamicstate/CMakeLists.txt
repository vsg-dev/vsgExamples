set(SOURCES vsgdynamicstate.cpp)

add_executable(vsgdynamicstate ${SOURCES})
set_target_properties(vsgdynamicstate PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgdynamicstate vsg::vsg vsgImGui::vsgImGui)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgdynamicstate PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgdynamicstate vsgXchange::vsgXchange)
endif()

install(TARGETS vsgdynamicstate RUNTIME DESTINATION bin)
