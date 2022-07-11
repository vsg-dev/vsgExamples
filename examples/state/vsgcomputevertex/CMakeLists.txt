set(SOURCES vsgcomputevertex.cpp)

add_executable(vsgcomputevertex ${SOURCES})

target_link_libraries(vsgcomputevertex vsg::vsg)

install(TARGETS vsgcomputevertex RUNTIME DESTINATION bin)
