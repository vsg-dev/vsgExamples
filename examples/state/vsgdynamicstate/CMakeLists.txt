set(SOURCES vsgdynamicstate.cpp)

add_executable(vsgdynamicstate ${SOURCES})

target_link_libraries(vsgdynamicstate vsg::vsg vsgImGui::vsgImGui)

install(TARGETS vsgdynamicstate RUNTIME DESTINATION bin)
