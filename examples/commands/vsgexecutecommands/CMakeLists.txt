set(SOURCES
    vsgexecutecommands.cpp
)

add_executable(vsgexecutecommands ${SOURCES})
set_target_properties(vsgexecutecommands PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgexecutecommands vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgexecutecommands PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgexecutecommands vsgXchange::vsgXchange)
endif()

install(TARGETS vsgexecutecommands RUNTIME DESTINATION bin)
