set(SOURCES
    vsgclip.cpp
)

add_executable(vsgclip ${SOURCES})

target_link_libraries(vsgclip vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgclip PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgclip vsgXchange::vsgXchange)
endif()

install(TARGETS vsgclip RUNTIME DESTINATION bin)
