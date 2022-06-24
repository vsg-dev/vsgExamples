set(SOURCES vsglog_mt.cpp)

add_executable(vsglog_mt ${SOURCES})

target_link_libraries(vsglog_mt vsg::vsg)

install(TARGETS vsglog_mt RUNTIME DESTINATION bin)
