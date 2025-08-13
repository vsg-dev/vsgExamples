set(SOURCES vsgdynamictexture_cs.cpp)

add_executable(vsgdynamictexture_cs ${SOURCES})
set_target_properties(vsgdynamictexture_cs PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgdynamictexture_cs vsg::vsg)

install(TARGETS vsgdynamictexture_cs RUNTIME DESTINATION bin)
