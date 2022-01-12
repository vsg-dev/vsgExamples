set(SOURCES vsgvisitorcustomtype.cpp)

add_executable(vsgvisitorcustomtype ${SOURCES})

target_link_libraries(vsgvisitorcustomtype vsg::vsg)

install(TARGETS vsgvisitorcustomtype RUNTIME DESTINATION bin)
