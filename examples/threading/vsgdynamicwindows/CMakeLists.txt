set(SOURCES
    vsgdynamicwindows.cpp
)

add_executable(vsgdynamicwindows ${SOURCES})

target_link_libraries(vsgdynamicwindows vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgdynamicwindows PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgdynamicwindows vsgXchange::vsgXchange)
endif()

install(TARGETS vsgdynamicwindows RUNTIME DESTINATION bin)
