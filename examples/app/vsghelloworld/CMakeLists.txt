set(SOURCES
    vsghelloworld.cpp
)

add_executable(vsghelloworld ${SOURCES})
set_target_properties(vsghelloworld PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsghelloworld vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsghelloworld PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsghelloworld vsgXchange::vsgXchange)
endif()

install(TARGETS vsghelloworld RUNTIME DESTINATION bin)
