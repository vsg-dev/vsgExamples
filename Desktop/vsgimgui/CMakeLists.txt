set(SOURCES
    vsgimgui.cpp
)

add_executable(vsgimgui ${SOURCES})

target_link_libraries(vsgimgui vsgImGui::vsgImGui vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgimgui PRIVATE USE_VSGXCHANGE)
    target_link_libraries(vsgimgui vsgXchange::vsgXchange)
endif()
