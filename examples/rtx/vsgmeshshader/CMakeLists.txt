set(SOURCES
    vsgmeshshader.cpp
)

add_executable(vsgmeshshader ${SOURCES})

target_link_libraries(vsgmeshshader vsg::vsg)
