set(SOURCES
    vsgallocationtests.cpp
)

add_executable(vsgallocationtests ${SOURCES})

target_link_libraries(vsgallocationtests vsg::vsg vsgXchange::vsgXchange)

install(TARGETS vsgallocationtests RUNTIME DESTINATION bin)
