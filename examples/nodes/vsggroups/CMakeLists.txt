set(SOURCES SharedPtrNode.cpp vsggroups.cpp)

add_executable(vsggroups ${HEADERS} ${SOURCES})
target_link_libraries(vsggroups vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsggroups PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsggroups vsgXchange::vsgXchange)
endif()

install(TARGETS vsggroups RUNTIME DESTINATION bin)
