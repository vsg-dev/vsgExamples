set(SOURCES
    vsgwindows.cpp
)

add_executable(vsgwindows ${SOURCES})

target_link_libraries(vsgwindows vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgwindows PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgwindows vsgXchange::vsgXchange)
endif()

install(TARGETS vsgwindows RUNTIME DESTINATION bin)
