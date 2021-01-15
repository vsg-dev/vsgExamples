set(SOURCES
    ../../shared/AnimationPath.cpp
    vsgheadless.cpp
)

add_executable(vsgheadless ${SOURCES})

target_link_libraries(vsgheadless vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgheadless PRIVATE USE_VSGXCHANGE)
    target_link_libraries(vsgheadless vsgXchange::vsgXchange)
endif()
