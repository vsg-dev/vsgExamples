set(SOURCES vsgtriangles.cpp)

add_executable(vsgtriangles ${SOURCES})

target_link_libraries(vsgtriangles vsg::vsg)

install(TARGETS vsgtriangles RUNTIME DESTINATION bin)
