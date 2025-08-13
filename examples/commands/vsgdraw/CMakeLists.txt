set(SOURCES vsgdraw.cpp)

add_executable(vsgdraw ${SOURCES})
set_target_properties(vsgdraw PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgdraw vsg::vsg)

install(TARGETS vsgdraw RUNTIME DESTINATION bin)
