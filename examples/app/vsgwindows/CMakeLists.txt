set(SOURCES
    vsgwindows.cpp
)

add_executable(vsgwindows ${SOURCES})
set_target_properties(vsgwindows PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgwindows vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgwindows PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgwindows vsgXchange::vsgXchange)
endif()

install(TARGETS vsgwindows RUNTIME DESTINATION bin)
