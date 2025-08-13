set(SOURCES
    vsgvolume.cpp
)

add_executable(vsgvolume ${SOURCES})
set_target_properties(vsgvolume PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgvolume vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgvolume PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgvolume vsgXchange::vsgXchange)
endif()

install(TARGETS vsgvolume RUNTIME DESTINATION bin)
