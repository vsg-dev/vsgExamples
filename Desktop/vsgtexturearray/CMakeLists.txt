set(SOURCES vsgtexturearray.cpp)

add_executable(vsgtexturearray ${SOURCES})

target_link_libraries(vsgtexturearray vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgtexturearray PRIVATE USE_VSGXCHANGE)
    target_link_libraries(vsgtexturearray vsgXchange::vsgXchange)
endif()
