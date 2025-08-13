set(SOURCES vsglog_mt.cpp)

add_executable(vsglog_mt ${SOURCES})
set_target_properties(vsglog_mt PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsglog_mt vsg::vsg)

install(TARGETS vsglog_mt RUNTIME DESTINATION bin)
