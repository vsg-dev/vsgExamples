set(SOURCES vsgvisitor.cpp)

add_executable(vsgvisitor ${SOURCES})
set_target_properties(vsgvisitor PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgvisitor vsg::vsg)

install(TARGETS vsgvisitor RUNTIME DESTINATION bin)
