set(SOURCES vsgskybox.cpp)

add_executable(vsgskybox ${SOURCES})
set_target_properties(vsgskybox PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgskybox vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgskybox PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgskybox vsgXchange::vsgXchange)
endif()

install(TARGETS vsgskybox RUNTIME DESTINATION bin)
