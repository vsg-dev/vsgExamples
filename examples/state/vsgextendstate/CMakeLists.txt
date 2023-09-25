set(SOURCES vsgextendstate.cpp)

add_executable(vsgextendstate ${SOURCES})

target_link_libraries(vsgextendstate vsg::vsg)

install(TARGETS vsgextendstate RUNTIME DESTINATION bin)
