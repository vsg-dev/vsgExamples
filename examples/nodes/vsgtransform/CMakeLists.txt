set(SOURCES
    vsgtransform.cpp
)

add_executable(vsgtransform ${SOURCES})

target_link_libraries(vsgtransform vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgtransform PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgtransform vsgXchange::vsgXchange)
endif()

install(TARGETS vsgtransform RUNTIME DESTINATION bin)
