set(SOURCES
    text.cpp
    flat.cpp
    phong.cpp
    pbr.cpp
    vsgshaderset.cpp
)

add_executable(vsgshaderset ${SOURCES})
set_target_properties(vsgshaderset PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgshaderset vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgshaderset PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgshaderset vsgXchange::vsgXchange)
endif()

install(TARGETS vsgshaderset RUNTIME DESTINATION bin)
