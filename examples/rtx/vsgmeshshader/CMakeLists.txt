set(SOURCES
    vsgmeshshader.cpp
)

add_executable(vsgmeshshader ${SOURCES})

target_link_libraries(vsgmeshshader vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgmeshshader PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgmeshshader vsgXchange::vsgXchange)
endif()

install(TARGETS vsgmeshshader RUNTIME DESTINATION bin)
