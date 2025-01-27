set(SOURCES
    vsgcolorspace.cpp
)

add_executable(vsgcolorspace ${SOURCES})

target_link_libraries(vsgcolorspace vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgcolorspace PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgcolorspace vsgXchange::vsgXchange)
endif()

install(TARGETS vsgcolorspace RUNTIME DESTINATION bin)
