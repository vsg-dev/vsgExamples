set(SOURCES vsgterrain.cpp)

add_executable(vsgterrain ${SOURCES})

target_link_libraries(vsgterrain vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgterrain PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgterrain vsgXchange::vsgXchange)
endif()

install(TARGETS vsgterrain RUNTIME DESTINATION bin)
