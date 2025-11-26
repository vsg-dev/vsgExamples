set(SOURCES
    vsgcast.cpp
)

add_executable(vsgcast ${SOURCES})

target_link_libraries(vsgcast vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgcast PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgcast vsgXchange::vsgXchange)
endif()

install(TARGETS vsgcast RUNTIME DESTINATION bin)
