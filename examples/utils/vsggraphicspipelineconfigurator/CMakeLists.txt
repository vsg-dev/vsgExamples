set(SOURCES
    vsggraphicspipelineconfigurator.cpp
)

add_executable(vsggraphicspipelineconfigurator ${SOURCES})
set_target_properties(vsggraphicspipelineconfigurator PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsggraphicspipelineconfigurator vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsggraphicspipelineconfigurator PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsggraphicspipelineconfigurator vsgXchange::vsgXchange)
endif()

install(TARGETS vsggraphicspipelineconfigurator RUNTIME DESTINATION bin)
