set(SOURCES
    vsgcolorspace.cpp
)

add_executable(vsgcolorspace ${SOURCES})
set_target_properties(vsgcolorspace PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgcolorspace vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgcolorspace PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgcolorspace vsgXchange::vsgXchange)
endif()

install(TARGETS vsgcolorspace RUNTIME DESTINATION bin)
