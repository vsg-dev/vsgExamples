set(SOURCES vsgpath.cpp)

add_executable(vsgpath ${SOURCES})
set_target_properties(vsgpath PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgpath vsg::vsg)

install(TARGETS vsgpath RUNTIME DESTINATION bin)
