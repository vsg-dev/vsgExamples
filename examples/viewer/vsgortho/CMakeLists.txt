set(SOURCES
    vsgortho.cpp
)

add_executable(vsgortho ${SOURCES})

target_link_libraries(vsgortho vsg::vsg)

install(TARGETS vsgortho RUNTIME DESTINATION bin)
