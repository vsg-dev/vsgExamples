set(SOURCES
    vsgoverlay.cpp
)

add_executable(vsgoverlay ${SOURCES})

target_link_libraries(vsgoverlay vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgoverlay PRIVATE USE_VSGXCHANGE)
    target_link_libraries(vsgoverlay vsgXchange::vsgXchange)
endif()
