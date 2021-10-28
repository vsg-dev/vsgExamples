set(SOURCES
    vsgpoints.cpp
)

add_executable(vsgpoints ${SOURCES})

target_link_libraries(vsgpoints vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgpoints PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgpoints vsgXchange::vsgXchange)
endif()

install(TARGETS vsgpoints RUNTIME DESTINATION bin)
