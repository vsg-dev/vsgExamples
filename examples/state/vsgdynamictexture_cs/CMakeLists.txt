set(SOURCES vsgdynamictexture_cs.cpp)

add_executable(vsgdynamictexture_cs ${SOURCES})

target_link_libraries(vsgdynamictexture_cs vsg::vsg)

install(TARGETS vsgdynamictexture_cs RUNTIME DESTINATION bin)
