set(SOURCES
    ../vsgviewer/AnimationPath.cpp
    vsgmultigpu.cpp
)

add_executable(vsgmultigpu ${SOURCES})

target_include_directories(vsgmultigpu PRIVATE ../vsgviewer)

target_link_libraries(vsgmultigpu vsg::vsg)
