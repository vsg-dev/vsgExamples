set(SOURCES
    vsgallocator.cpp
)

add_executable(vsgallocator ${SOURCES})
set_target_properties(vsgallocator PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgallocator vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgallocator PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgallocator vsgXchange::vsgXchange)
endif()

install(TARGETS vsgallocator RUNTIME DESTINATION bin)
