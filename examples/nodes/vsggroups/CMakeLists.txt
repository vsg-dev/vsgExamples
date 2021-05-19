set(HEADERS SharedPtrNode.h)
set(SOURCES SharedPtrNode.cpp vsggroups.cpp)

add_executable(vsggroups ${HEADERS} ${SOURCES})
target_link_libraries(vsggroups vsg::vsg)

install(TARGETS vsggroups RUNTIME DESTINATION bin)
