set(SOURCES vsgraytracing_advanced.cpp)

add_executable(vsgraytracing_advanced ${SOURCES})

target_link_libraries(vsgraytracing_advanced vsg::vsg)
