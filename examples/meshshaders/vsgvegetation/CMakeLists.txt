set(SOURCES
    vsgvegetation.cpp
)

add_executable(vsgvegetation ${SOURCES})

target_link_libraries(vsgvegetation vsg::vsg)

target_compile_definitions(vsgvegetation PRIVATE vsgXchange_FOUND)
target_link_libraries(vsgvegetation vsgXchange::vsgXchange)

install(TARGETS vsgvegetation RUNTIME DESTINATION bin)
