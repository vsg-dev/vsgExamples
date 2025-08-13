set(SOURCES
    custom_phong.cpp
    vsgtextureprojection.cpp
)

add_executable(vsgtextureprojection ${SOURCES})
set_target_properties(vsgtextureprojection PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgtextureprojection vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgtextureprojection PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgtextureprojection vsgXchange::vsgXchange)
endif()

install(TARGETS vsgtextureprojection RUNTIME DESTINATION bin)
