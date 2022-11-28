set(SOURCES
    vsgviewer.cpp
)

add_executable(vsgviewer ${SOURCES})

target_link_libraries(vsgviewer vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgviewer PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgviewer vsgXchange::vsgXchange)
endif()

if(vsgImGui_FOUND)
    target_compile_definitions(vsgviewer PRIVATE vsgImGui_FOUND)
    target_link_libraries(vsgviewer vsgImGui::vsgImGui)
endif()

install(TARGETS vsgviewer RUNTIME DESTINATION bin)
