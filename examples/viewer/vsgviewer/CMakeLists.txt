set(SOURCES
    ../../shared/AnimationPath.cpp
    vsgviewer.cpp
)

add_executable(vsgviewer ${SOURCES})

target_link_libraries(vsgviewer vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgviewer PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgviewer vsgXchange::vsgXchange)
endif()

install(TARGETS vsgviewer RUNTIME DESTINATION bin)
