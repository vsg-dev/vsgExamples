set(SOURCES
    vsgallocator.cpp
)

add_executable(vsgallocator ${SOURCES})

target_link_libraries(vsgallocator vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgallocator PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgallocator vsgXchange::vsgXchange)
endif()

install(TARGETS vsgallocator RUNTIME DESTINATION bin)
