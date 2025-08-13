set(SOURCES vsglog.cpp)

add_executable(vsglog ${SOURCES})
set_target_properties(vsglog PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsglog vsg::vsg)

install(TARGETS vsglog RUNTIME DESTINATION bin)
