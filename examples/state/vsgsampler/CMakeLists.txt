set(SOURCES vsgsampler.cpp)

add_executable(vsgsampler ${SOURCES})

target_link_libraries(vsgsampler vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgsampler PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgsampler vsgXchange::vsgXchange)
endif()

install(TARGETS vsgsampler RUNTIME DESTINATION bin)
