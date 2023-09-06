set(SOURCES
    vsgrendertotexturearray.cpp
)

add_executable(vsgrendertotexturearray ${SOURCES})

target_link_libraries(vsgrendertotexturearray vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgrendertotexturearray PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgrendertotexturearray vsgXchange::vsgXchange)
endif()

install(TARGETS vsgrendertotexturearray RUNTIME DESTINATION bin)
