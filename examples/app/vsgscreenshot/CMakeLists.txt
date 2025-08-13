set(SOURCES
    vsgscreenshot.cpp
)

add_executable(vsgscreenshot ${SOURCES})
set_target_properties(vsgscreenshot PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgscreenshot vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgscreenshot PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgscreenshot vsgXchange::vsgXchange)
endif()

install(TARGETS vsgscreenshot RUNTIME DESTINATION bin)
