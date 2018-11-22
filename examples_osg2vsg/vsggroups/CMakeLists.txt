set(HEADERS SharedPtrNode.h)
set(SOURCES SharedPtrNode.cpp vsggroups.cpp)

add_executable(vsggroups ${HEADERS} ${SOURCES})
target_include_directories(vsggroups PRIVATE ${OSG_INCLUDE_DIR})
target_link_libraries(vsggroups vsg::vsg ${OSG_LIBRARIES})
