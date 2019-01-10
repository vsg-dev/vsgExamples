set(SOURCES
    Trackball.cpp
    CreateSceneData.cpp
    vsgcamera.cpp
)

add_executable(vsgcamera ${SOURCES})

target_link_libraries(vsgcamera vsg::vsg)
