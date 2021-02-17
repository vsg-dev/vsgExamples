set(SOURCES
    vsgdynamicload.cpp
)

add_executable(vsgdynamicload ${SOURCES})

target_link_libraries(vsgdynamicload vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgdynamicload PRIVATE USE_VSGXCHANGE)
    target_link_libraries(vsgdynamicload vsgXchange::vsgXchange)
endif()
