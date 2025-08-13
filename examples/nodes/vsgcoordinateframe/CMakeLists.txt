set(SOURCES vsgcoordinateframe.cpp)

add_executable(vsgcoordinateframe ${HEADERS} ${SOURCES})
set_target_properties(vsgcoordinateframe PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgcoordinateframe vsg::vsg)

target_compile_definitions(vsgcoordinateframe PRIVATE vsgXchange_FOUND)
target_link_libraries(vsgcoordinateframe vsgXchange::vsgXchange)

install(TARGETS vsgcoordinateframe RUNTIME DESTINATION bin)
