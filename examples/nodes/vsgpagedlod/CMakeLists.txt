set(SOURCES
    ../../shared/AnimationPath.cpp
    vsgpagedlod.cpp
)

add_executable(vsgpagedlod ${SOURCES})

target_link_libraries(vsgpagedlod vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgpagedlod PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgpagedlod vsgXchange::vsgXchange)
endif()
