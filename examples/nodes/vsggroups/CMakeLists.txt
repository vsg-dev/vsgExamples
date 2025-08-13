set(HEADERS SharedPtrNode.h)
set(SOURCES SharedPtrNode.cpp vsggroups.cpp)

add_executable(vsggroups ${HEADERS} ${SOURCES})
set_target_properties(vsggroups PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsggroups vsg::vsg)

install(TARGETS vsggroups RUNTIME DESTINATION bin)
