set(SOURCES vsgallocator.cpp)

add_executable(vsgallocator ${SOURCES})

target_link_libraries(vsgallocator vsg::vsg)
