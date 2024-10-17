set(SOURCES
    vsgaxes.cpp
)

add_executable(vsgaxes ${SOURCES})

target_link_libraries(vsgaxes vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgaxes PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgaxes vsgXchange::vsgXchange)
endif()

install(TARGETS vsgaxes RUNTIME DESTINATION bin)
