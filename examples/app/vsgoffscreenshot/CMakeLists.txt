set(SOURCES
    vsgoffscreenshot.cpp
)

add_executable(vsgoffscreenshot ${SOURCES})

target_link_libraries(vsgoffscreenshot vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgoffscreenshot PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgoffscreenshot vsgXchange::vsgXchange)
endif()

install(TARGETS vsgoffscreenshot RUNTIME DESTINATION bin)
