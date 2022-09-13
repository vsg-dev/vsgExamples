set(SOURCES
    vsgdynamicvertex.cpp
)

add_executable(vsgdynamicvertex ${SOURCES})

target_link_libraries(vsgdynamicvertex vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgdynamicvertex PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgdynamicvertex vsgXchange::vsgXchange)
endif()

install(TARGETS vsgdynamicvertex RUNTIME DESTINATION bin)
