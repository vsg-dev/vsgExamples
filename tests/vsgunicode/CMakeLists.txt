set(SOURCES vsgunicode.cpp)

add_executable(vsgunicode ${SOURCES})
set_target_properties(vsgunicode PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgunicode vsg::vsg)

install(TARGETS vsgunicode RUNTIME DESTINATION bin)
