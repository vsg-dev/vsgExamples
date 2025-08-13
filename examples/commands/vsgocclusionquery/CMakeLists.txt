set(SOURCES vsgocclusionquery.cpp)

add_executable(vsgocclusionquery ${SOURCES})
set_target_properties(vsgocclusionquery PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgocclusionquery vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgocclusionquery PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgocclusionquery vsgXchange::vsgXchange)
endif()

install(TARGETS vsgocclusionquery RUNTIME DESTINATION bin)
