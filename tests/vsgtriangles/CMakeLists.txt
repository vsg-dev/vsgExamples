set(SOURCES vsgtriangles.cpp)

add_executable(vsgtriangles ${SOURCES})
set_target_properties(vsgtriangles PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgtriangles vsg::vsg)

install(TARGETS vsgtriangles RUNTIME DESTINATION bin)
