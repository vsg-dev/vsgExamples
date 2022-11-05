set(SOURCES
    vsganaglyphicstereo.cpp
)

add_executable(vsganaglyphicstereo ${SOURCES})

target_link_libraries(vsganaglyphicstereo vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsganaglyphicstereo PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsganaglyphicstereo vsgXchange::vsgXchange)
endif()

install(TARGETS vsganaglyphicstereo RUNTIME DESTINATION bin)
