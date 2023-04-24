set(SOURCES
    vsgvolume.cpp
)

add_executable(vsgvolume ${SOURCES})

target_link_libraries(vsgvolume vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgvolume PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgvolume vsgXchange::vsgXchange)
endif()

install(TARGETS vsgvolume RUNTIME DESTINATION bin)
