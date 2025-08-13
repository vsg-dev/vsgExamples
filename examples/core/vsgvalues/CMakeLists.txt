set(SOURCES vsgvalues.cpp)

add_executable(vsgvalues ${SOURCES})
set_target_properties(vsgvalues PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgvalues vsg::vsg)

install(TARGETS vsgvalues RUNTIME DESTINATION bin)
