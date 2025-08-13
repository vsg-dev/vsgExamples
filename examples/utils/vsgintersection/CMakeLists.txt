set(SOURCES
    vsgintersection.cpp
)

add_executable(vsgintersection ${SOURCES})
set_target_properties(vsgintersection PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgintersection vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgintersection PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgintersection vsgXchange::vsgXchange)
endif()

install(TARGETS vsgintersection RUNTIME DESTINATION bin)
