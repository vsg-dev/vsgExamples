set(SOURCES vsgskybox.cpp)

add_executable(vsgskybox ${SOURCES})

target_link_libraries(vsgskybox vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgskybox PRIVATE USE_VSGXCHANGE)
    target_link_libraries(vsgskybox vsgXchange::vsgXchange)
endif()

if(WIN32)
    add_custom_command(TARGET vsgskybox 
        POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:vsg::vsg> $<TARGET_FILE_DIR:vsgskybox>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:vsgXchange::vsgXchange> $<TARGET_FILE_DIR:vsgskybox>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:KTX::ktx> $<TARGET_FILE_DIR:vsgskybox>
)
endif()
