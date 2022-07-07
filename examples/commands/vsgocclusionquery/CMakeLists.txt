set(SOURCES vsgocclusionquery.cpp)

add_executable(vsgocclusionquery ${SOURCES})

target_link_libraries(vsgocclusionquery vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgocclusionquery PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgocclusionquery vsgXchange::vsgXchange)
endif()

install(TARGETS vsgocclusionquery RUNTIME DESTINATION bin)
