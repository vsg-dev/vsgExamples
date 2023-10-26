set(SOURCES
    custom_pbr.cpp
    vsgcustomshaderset.cpp
)

add_executable(vsgcustomshaderset ${SOURCES})

target_link_libraries(vsgcustomshaderset vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgcustomshaderset PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgcustomshaderset vsgXchange::vsgXchange)
endif()

install(TARGETS vsgcustomshaderset RUNTIME DESTINATION bin)
