set(SOURCES
    vsgcustomtext.cpp
)

add_executable(vsgcustomtext ${SOURCES})

target_link_libraries(vsgcustomtext vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgcustomtext PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgcustomtext vsgXchange::vsgXchange)
endif()

install(TARGETS vsgcustomtext RUNTIME DESTINATION bin)
