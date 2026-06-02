set(SOURCES
    vsgmeshshaderset.cpp
)


add_executable(vsgmeshshaderset ${SOURCES})

target_link_libraries(vsgmeshshaderset vsg::vsg)

target_compile_definitions(vsgmeshshaderset PRIVATE vsgXchange_FOUND)
target_link_libraries(vsgmeshshaderset vsgXchange::vsgXchange)

if (meshoptimizer_FOUND)
    target_compile_definitions(vsgmeshshaderset PRIVATE meshoptimizer_FOUND)
    target_link_libraries(vsgmeshshaderset meshoptimizer::meshoptimizer)
endif()

install(TARGETS vsgmeshshaderset RUNTIME DESTINATION bin)
