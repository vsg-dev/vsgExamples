set(SOURCES
    vsgmeshshaderset.cpp
)

add_executable(vsgmeshshaderset ${SOURCES})

target_link_libraries(vsgmeshshaderset vsg::vsg)

target_compile_definitions(vsgmeshshaderset PRIVATE vsgXchange_FOUND)
target_link_libraries(vsgmeshshaderset vsgXchange::vsgXchange)

install(TARGETS vsgmeshshaderset RUNTIME DESTINATION bin)
