set(SOURCES
    vsgstateswitch.cpp
)

add_executable(vsgstateswitch ${SOURCES})

target_link_libraries(vsgstateswitch vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgstateswitch PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgstateswitch vsgXchange::vsgXchange)
endif()

install(TARGETS vsgstateswitch RUNTIME DESTINATION bin)
