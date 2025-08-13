set(SOURCES
    vsgbuilder.cpp
)

add_executable(vsgbuilder ${SOURCES})
set_target_properties(vsgbuilder PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgbuilder vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgbuilder PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgbuilder vsgXchange::vsgXchange)
endif()

install(TARGETS vsgbuilder RUNTIME DESTINATION bin)
