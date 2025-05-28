set(SOURCES vsgunicode.cpp)

add_executable(vsgunicode ${SOURCES})

target_link_libraries(vsgunicode vsg::vsg)

install(TARGETS vsgunicode RUNTIME DESTINATION bin)
