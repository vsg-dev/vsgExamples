set(SOURCES
    custom_pbr.cpp
    vsgincompatiblepipelinelayouts.cpp
)

add_executable(vsgincompatiblepipelinelayouts ${SOURCES})

target_link_libraries(vsgincompatiblepipelinelayouts vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgincompatiblepipelinelayouts PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgincompatiblepipelinelayouts vsgXchange::vsgXchange)
endif()

install(TARGETS vsgincompatiblepipelinelayouts RUNTIME DESTINATION bin)
