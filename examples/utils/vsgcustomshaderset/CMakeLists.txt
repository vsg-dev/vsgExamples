set(SOURCES
    custom_pbr.cpp
    vsgcustomshaderset.cpp
)

add_executable(vsgcustomshaderset ${SOURCES})
set_target_properties(vsgcustomshaderset PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgcustomshaderset vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgcustomshaderset PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgcustomshaderset vsgXchange::vsgXchange)
endif()

install(TARGETS vsgcustomshaderset RUNTIME DESTINATION bin)
