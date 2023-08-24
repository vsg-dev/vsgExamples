set(SOURCES
    vsgrendertotexture.cpp
)

add_executable(vsgrendertotexture ${SOURCES})

target_link_libraries(vsgrendertotexture vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgrendertotexture PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgrendertotexture vsgXchange::vsgXchange)
endif()

install(TARGETS vsgrendertotexture RUNTIME DESTINATION bin)
