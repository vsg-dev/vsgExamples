set(SOURCES
    vsglayers.cpp
)

add_executable(vsglayers ${SOURCES})

target_link_libraries(vsglayers vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsglayers PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsglayers vsgXchange::vsgXchange)
endif()

install(TARGETS vsglayers RUNTIME DESTINATION bin)
