set(SOURCES vsgdynamictexture.cpp)

add_executable(vsgdynamictexture ${SOURCES})

target_link_libraries(vsgdynamictexture vsg::vsg)
