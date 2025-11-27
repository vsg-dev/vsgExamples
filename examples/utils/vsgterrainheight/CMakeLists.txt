set(SOURCES
    vsgterrainheight.cpp
)

add_executable(vsgterrainheight ${SOURCES})

target_link_libraries(vsgterrainheight vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgterrainheight PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgterrainheight vsgXchange::vsgXchange)
endif()

install(TARGETS vsgterrainheight RUNTIME DESTINATION bin)
