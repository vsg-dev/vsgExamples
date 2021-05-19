set(SOURCES
    ../../shared/AnimationPath.cpp
    vsgmultigpu.cpp
)

add_executable(vsgmultigpu ${SOURCES})

target_link_libraries(vsgmultigpu vsg::vsg)

install(TARGETS vsgmultigpu RUNTIME DESTINATION bin)
