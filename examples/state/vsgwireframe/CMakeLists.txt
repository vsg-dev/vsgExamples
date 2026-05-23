set(SOURCES
    vsgwireframe.cpp
)

add_executable(vsgwireframe ${SOURCES})

target_link_libraries(vsgwireframe vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgwireframe PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgwireframe vsgXchange::vsgXchange)
endif()

install(TARGETS vsgwireframe RUNTIME DESTINATION bin)
