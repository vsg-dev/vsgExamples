set(SOURCES
    vsgortho.cpp
)

add_executable(vsgortho ${SOURCES})

target_link_libraries(vsgortho vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgortho PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgortho vsgXchange::vsgXchange)
endif()

install(TARGETS vsgortho RUNTIME DESTINATION bin)
