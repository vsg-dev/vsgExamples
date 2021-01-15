set(SOURCES
    ../../shared/AnimationPath.cpp
    vsgscreenshot.cpp
)

add_executable(vsgscreenshot ${SOURCES})

target_link_libraries(vsgscreenshot vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgscreenshot PRIVATE USE_VSGXCHANGE)
    target_link_libraries(vsgscreenshot vsgXchange::vsgXchange)
endif()
