set(SOURCES
    vsgmeshshader.cpp
)

add_executable(vsgmeshshader ${SOURCES})
set_target_properties(vsgmeshshader PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgmeshshader vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgmeshshader PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgmeshshader vsgXchange::vsgXchange)
endif()

install(TARGETS vsgmeshshader RUNTIME DESTINATION bin)
