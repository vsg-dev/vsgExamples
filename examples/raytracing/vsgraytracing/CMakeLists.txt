set(SOURCES
    vsgraytracing.cpp
)

add_executable(vsgraytracing ${SOURCES})
set_target_properties(vsgraytracing PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgraytracing vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgraytracing PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgraytracing vsgXchange::vsgXchange)
endif()

install(TARGETS vsgraytracing RUNTIME DESTINATION bin)
