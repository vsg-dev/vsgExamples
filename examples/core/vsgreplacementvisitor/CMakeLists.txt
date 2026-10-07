set(SOURCES vsgreplacementvisitor.cpp)

add_executable(vsgreplacementvisitor ${SOURCES})

target_link_libraries(vsgreplacementvisitor vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgreplacementvisitor PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgreplacementvisitor vsgXchange::vsgXchange)
endif()

install(TARGETS vsgreplacementvisitor RUNTIME DESTINATION bin)
