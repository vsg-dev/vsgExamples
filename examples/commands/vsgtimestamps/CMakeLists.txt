set(SOURCES vsgtimestamps.cpp)

add_executable(vsgtimestamps ${SOURCES})

target_link_libraries(vsgtimestamps vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgtimestamps PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgtimestamps vsgXchange::vsgXchange)
endif()

install(TARGETS vsgtimestamps RUNTIME DESTINATION bin)
