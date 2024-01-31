set(SOURCES
    vsgmorph.cpp
)

add_executable(vsgmorph ${SOURCES})

target_link_libraries(vsgmorph vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgmorph PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgmorph vsgXchange::vsgXchange)
endif()

install(TARGETS vsgmorph RUNTIME DESTINATION bin)
