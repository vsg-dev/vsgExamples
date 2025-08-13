set(SOURCES
    vsgrendertotexture.cpp
)

add_executable(vsgrendertotexture ${SOURCES})
set_target_properties(vsgrendertotexture PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgrendertotexture vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgrendertotexture PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgrendertotexture vsgXchange::vsgXchange)
endif()

install(TARGETS vsgrendertotexture RUNTIME DESTINATION bin)
