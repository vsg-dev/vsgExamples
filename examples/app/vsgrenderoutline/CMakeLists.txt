set(SOURCES
    vsgrenderoutline.cpp
)

add_executable(vsgrenderoutline ${SOURCES})

target_link_libraries(vsgrenderoutline vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgrenderoutline PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgrenderoutline vsgXchange::vsgXchange)
endif()

install(TARGETS vsgrenderoutline RUNTIME DESTINATION bin)
