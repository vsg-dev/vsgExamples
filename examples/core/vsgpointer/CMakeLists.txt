set(SOURCES vsgpointer.cpp)

add_executable(vsgpointer ${SOURCES})
set_target_properties(vsgpointer PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgpointer vsg::vsg)

install(TARGETS vsgpointer RUNTIME DESTINATION bin)
