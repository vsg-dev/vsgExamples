set(SOURCES vsgobjects.cpp)

add_executable(vsgobjects ${SOURCES})
target_include_directories(vsgobjects PRIVATE ${OSG_INCLUDE_DIR})
target_link_libraries(vsgobjects vsg::vsg ${OSG_LIBRARIES})
