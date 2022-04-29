set(SOURCES
    vsgstatecomposer.cpp
)

add_executable(vsgstatecomposer ${SOURCES})

target_link_libraries(vsgstatecomposer vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgstatecomposer PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgstatecomposer vsgXchange::vsgXchange)
endif()

install(TARGETS vsgstatecomposer RUNTIME DESTINATION bin)
