set(SOURCES vsgvalues.cpp)

add_executable(vsgvalues ${SOURCES})

target_link_libraries(vsgvalues vsg::vsg)
