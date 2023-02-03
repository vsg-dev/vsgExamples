set(SOURCES
    Document.cpp
    vsgpresent.cpp
)

add_executable(vsgpresent ${SOURCES})

target_link_libraries(vsgpresent vsg::vsg vsgXchange::vsgXchange)

install(TARGETS vsgpresent RUNTIME DESTINATION bin)
