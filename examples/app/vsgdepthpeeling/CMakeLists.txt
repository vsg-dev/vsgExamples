set(SOURCES
    depthpeeling/Bindings.cpp
    depthpeeling/Bindings.h
    depthpeeling/Builder.cpp
    depthpeeling/Builder.h
    depthpeeling/PipelineConfigurator.cpp
    depthpeeling/PipelineConfigurator.h
    depthpeeling/RenderGraph.cpp
    depthpeeling/RenderGraph.h
    depthpeeling/Resources.cpp
    depthpeeling/Resources.h
    depthpeeling/ShaderSet.cpp
    depthpeeling/ShaderSet.h
    vsgdepthpeeling.cpp
)

add_executable(vsgdepthpeeling ${SOURCES})

target_link_libraries(vsgdepthpeeling vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgdepthpeeling PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgdepthpeeling vsgXchange::vsgXchange)
endif()

install(TARGETS vsgdepthpeeling RUNTIME DESTINATION bin)
