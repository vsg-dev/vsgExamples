set(SOURCES
    vsgoffscreenshot.cpp
)

add_executable(vsgoffscreenshot ${SOURCES})
set_target_properties(vsgoffscreenshot PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgoffscreenshot vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgoffscreenshot PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgoffscreenshot vsgXchange::vsgXchange)
endif()

install(TARGETS vsgoffscreenshot RUNTIME DESTINATION bin)
