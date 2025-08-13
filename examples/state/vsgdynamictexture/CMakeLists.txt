set(SOURCES vsgdynamictexture.cpp)

add_executable(vsgdynamictexture ${SOURCES})
set_target_properties(vsgdynamictexture PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgdynamictexture vsg::vsg)

install(TARGETS vsgdynamictexture RUNTIME DESTINATION bin)
