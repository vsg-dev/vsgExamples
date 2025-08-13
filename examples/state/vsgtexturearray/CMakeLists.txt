set(SOURCES vsgtexturearray.cpp)

add_executable(vsgtexturearray ${SOURCES})
set_target_properties(vsgtexturearray PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgtexturearray vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgtexturearray PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgtexturearray vsgXchange::vsgXchange)
endif()

install(TARGETS vsgtexturearray RUNTIME DESTINATION bin)
