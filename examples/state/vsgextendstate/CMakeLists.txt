set(SOURCES vsgextendstate.cpp)

add_executable(vsgextendstate ${SOURCES})
set_target_properties(vsgextendstate PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgextendstate vsg::vsg)

install(TARGETS vsgextendstate RUNTIME DESTINATION bin)
