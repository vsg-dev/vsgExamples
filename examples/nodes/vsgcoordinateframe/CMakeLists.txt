set(SOURCES vsgcoordinateframe.cpp)

add_executable(vsgcoordinateframe ${HEADERS} ${SOURCES})
target_link_libraries(vsgcoordinateframe vsg::vsg)

target_compile_definitions(vsgcoordinateframe PRIVATE vsgXchange_FOUND)
target_link_libraries(vsgcoordinateframe vsgXchange::vsgXchange)

install(TARGETS vsgcoordinateframe RUNTIME DESTINATION bin)
