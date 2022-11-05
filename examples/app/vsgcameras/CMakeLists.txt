set(SOURCES
    vsgcameras.cpp
)

add_executable(vsgcameras ${SOURCES})

target_link_libraries(vsgcameras vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgcameras PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgcameras vsgXchange::vsgXchange)
endif()

install(TARGETS vsgcameras RUNTIME DESTINATION bin)
