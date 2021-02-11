set(SOURCES vsgskybox.cpp)

add_executable(vsgskybox ${SOURCES})

target_link_libraries(vsgskybox vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgskybox PRIVATE USE_VSGXCHANGE)
    target_link_libraries(vsgskybox vsgXchange::vsgXchange)
endif()
