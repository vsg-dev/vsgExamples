set(SOURCES
    ../../shared/AnimationPath.cpp
    vsgviewer.cpp
)

add_executable(vsgviewer ${SOURCES})

target_link_libraries(vsgviewer vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgviewer PRIVATE USE_VSGXCHANGE)
    target_link_libraries(vsgviewer vsgXchange::vsgXchange)
endif()

if (vsgGIS_FOUND)
    target_compile_definitions(vsgviewer PRIVATE USE_VSGGIS)
    target_link_libraries(vsgviewer vsgGIS::vsgGIS)
endif()
