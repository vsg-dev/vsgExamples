set(SOURCES
    vsgvalidate.cpp
)

add_executable(vsgvalidate ${SOURCES})
set_target_properties(vsgvalidate PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgvalidate vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgvalidate PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgvalidate vsgXchange::vsgXchange)
endif()

install(TARGETS vsgvalidate RUNTIME DESTINATION bin)
