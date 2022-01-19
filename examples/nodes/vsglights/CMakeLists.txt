set(SOURCES
    vsglights.cpp
)

add_executable(vsglights ${SOURCES})

target_link_libraries(vsglights vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsglights PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsglights vsgXchange::vsgXchange)
endif()

install(TARGETS vsglights RUNTIME DESTINATION bin)
