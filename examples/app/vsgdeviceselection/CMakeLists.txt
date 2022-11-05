set(SOURCES
    vsgdeviceselection.cpp
)

add_executable(vsgdeviceselection ${SOURCES})

target_link_libraries(vsgdeviceselection vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgdeviceselection PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgdeviceselection vsgXchange::vsgXchange)
endif()

install(TARGETS vsgdeviceselection RUNTIME DESTINATION bin)
