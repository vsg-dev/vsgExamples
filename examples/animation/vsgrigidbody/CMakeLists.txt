set(SOURCES
    vsgrigidbody.cpp
)

add_executable(vsgrigidbody ${SOURCES})

target_link_libraries(vsgrigidbody vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgrigidbody PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgrigidbody vsgXchange::vsgXchange)
endif()

install(TARGETS vsgrigidbody RUNTIME DESTINATION bin)
