set(SOURCES
    vsgimgui_example.cpp
)

add_executable(vsgimgui_example ${SOURCES})

target_link_libraries(vsgimgui_example vsgImGui::vsgImGui vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgimgui_example PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgimgui_example vsgXchange::vsgXchange)
endif()

install(TARGETS vsgimgui_example RUNTIME DESTINATION bin)
