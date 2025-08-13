set(SOURCES
    vsgdynamicviews.cpp
)

add_executable(vsgdynamicviews ${SOURCES})
set_target_properties(vsgdynamicviews PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgdynamicviews vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgdynamicviews PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgdynamicviews vsgXchange::vsgXchange)
endif()

install(TARGETS vsgdynamicviews RUNTIME DESTINATION bin)
