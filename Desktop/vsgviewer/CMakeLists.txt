set(SOURCES
    AnimationPath.cpp
    vsgviewer.cpp
)

add_executable(vsgviewer ${SOURCES})

target_link_libraries(vsgviewer vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgviewer PRIVATE USE_VSGXCHANGE)
    target_link_libraries(vsgviewer vsgXchange::vsgXchange)
endif()
