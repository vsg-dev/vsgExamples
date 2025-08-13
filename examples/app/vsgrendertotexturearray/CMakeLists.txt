set(SOURCES
    vsgrendertotexturearray.cpp
)

add_executable(vsgrendertotexturearray ${SOURCES})
set_target_properties(vsgrendertotexturearray PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgrendertotexturearray vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgrendertotexturearray PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgrendertotexturearray vsgXchange::vsgXchange)
endif()

install(TARGETS vsgrendertotexturearray RUNTIME DESTINATION bin)
