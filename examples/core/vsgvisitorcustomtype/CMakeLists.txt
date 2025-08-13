set(SOURCES vsgvisitorcustomtype.cpp)

add_executable(vsgvisitorcustomtype ${SOURCES})
set_target_properties(vsgvisitorcustomtype PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgvisitorcustomtype vsg::vsg)

install(TARGETS vsgvisitorcustomtype RUNTIME DESTINATION bin)
