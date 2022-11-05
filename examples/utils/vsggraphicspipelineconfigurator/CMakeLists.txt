set(SOURCES
    vsggraphicspipelineconfigurator.cpp
)

add_executable(vsggraphicspipelineconfigurator ${SOURCES})

target_link_libraries(vsggraphicspipelineconfigurator vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsggraphicspipelineconfigurator PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsggraphicspipelineconfigurator vsgXchange::vsgXchange)
endif()

install(TARGETS vsggraphicspipelineconfigurator RUNTIME DESTINATION bin)
