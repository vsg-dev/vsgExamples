set(SOURCES
    vsgvalidate.cpp
)

add_executable(vsgvalidate ${SOURCES})

target_link_libraries(vsgvalidate vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgvalidate PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgvalidate vsgXchange::vsgXchange)
endif()

install(TARGETS vsgvalidate RUNTIME DESTINATION bin)
