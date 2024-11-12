set(SOURCES vsgcoordinateframe.cpp)

add_executable(vsgcoordinateframe ${HEADERS} ${SOURCES})
target_link_libraries(vsgcoordinateframe vsg::vsg)

install(TARGETS vsgcoordinateframe RUNTIME DESTINATION bin)
