set(SOURCES vsgquerypool.cpp)

add_executable(vsgquerypool ${SOURCES})

target_link_libraries(vsgquerypool vsg::vsg)

install(TARGETS vsgquerypool RUNTIME DESTINATION bin)
