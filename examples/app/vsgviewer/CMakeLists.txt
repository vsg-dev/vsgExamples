set(SOURCES
    vsgviewer.cpp
)

add_executable(vsgviewer ${SOURCES})
set_target_properties(vsgviewer PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgviewer vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgviewer PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgviewer vsgXchange::vsgXchange)
endif()

install(TARGETS vsgviewer RUNTIME DESTINATION bin)
