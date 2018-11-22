set(SOURCES vsgnodes.cpp)

add_executable(vsgnodes ${SOURCES})
target_include_directories(vsgnodes PRIVATE ${OSG_INCLUDE_DIR})
target_link_libraries(vsgnodes vsg::vsg ${OSG_LIBRARIES})
