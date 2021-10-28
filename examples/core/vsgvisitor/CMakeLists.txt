set(SOURCES vsgvisitor.cpp)

add_executable(vsgvisitor ${SOURCES})

target_link_libraries(vsgvisitor vsg::vsg)

install(TARGETS vsgvisitor RUNTIME DESTINATION bin)
