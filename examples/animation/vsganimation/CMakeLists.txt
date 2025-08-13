set(SOURCES
    vsganimation.cpp
)

add_executable(vsganimation ${SOURCES})
set_target_properties(vsganimation PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsganimation vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsganimation PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsganimation vsgXchange::vsgXchange)
endif()

if (Tracy_FOUND)
    target_compile_definitions(vsganimation PRIVATE Tracy_FOUND)
    target_link_libraries(vsganimation Tracy::TracyClient)
endif()

install(TARGETS vsganimation RUNTIME DESTINATION bin)
