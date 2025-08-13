set(SOURCES
    Turntable.cpp
    vsgturntablecamera.cpp
)

add_executable(vsgturntablecamera ${SOURCES})
set_target_properties(vsgturntablecamera PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgturntablecamera vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgturntablecamera PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgturntablecamera vsgXchange::vsgXchange)
endif()

install(TARGETS vsgturntablecamera RUNTIME DESTINATION bin)
