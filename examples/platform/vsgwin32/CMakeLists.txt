set(SOURCES
    vsgwin32.cpp
)

add_executable(vsgwin32 WIN32 ${SOURCES})
target_link_libraries(vsgwin32 vsg::vsg)

install(TARGETS vsgwin32 RUNTIME DESTINATION bin)
