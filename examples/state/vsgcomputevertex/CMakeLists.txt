set(SOURCES vsgcomputevertex.cpp)

add_executable(vsgcomputevertex ${SOURCES})
set_target_properties(vsgcomputevertex PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgcomputevertex vsg::vsg)

install(TARGETS vsgcomputevertex RUNTIME DESTINATION bin)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgcomputevertex PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgcomputevertex vsgXchange::vsgXchange)
endif()
