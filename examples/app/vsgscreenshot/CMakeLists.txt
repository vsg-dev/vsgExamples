set(SOURCES
    vsgscreenshot.cpp
)

add_executable(vsgscreenshot ${SOURCES})

target_link_libraries(vsgscreenshot vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgscreenshot PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgscreenshot vsgXchange::vsgXchange)
endif()

install(TARGETS vsgscreenshot RUNTIME DESTINATION bin)


add_executable(vsgscreenshot2 vsgscreenshot2.cpp)

target_link_libraries(vsgscreenshot2 vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgscreenshot2 PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgscreenshot2 vsgXchange::vsgXchange)
endif()

install(TARGETS vsgscreenshot2 RUNTIME DESTINATION bin)
