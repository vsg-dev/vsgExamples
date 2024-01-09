set(SOURCES
    vsgdynamicload.cpp
)

add_executable(vsgdynamicload ${SOURCES})

target_link_libraries(vsgdynamicload vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgdynamicload PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgdynamicload vsgXchange::vsgXchange)
endif()

if (Tracy_FOUND)
    target_compile_definitions(vsgdynamicload PRIVATE Tracy_FOUND)
    target_link_libraries(vsgdynamicload Tracy::TracyClient)
endif()

install(TARGETS vsgdynamicload RUNTIME DESTINATION bin)
