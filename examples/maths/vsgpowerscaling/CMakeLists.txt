set(SOURCES
    vsgpowerscaling.cpp
)

add_executable(vsgpowerscaling ${SOURCES})

target_link_libraries(vsgpowerscaling )

target_link_libraries(vsgpowerscaling vsg::vsg vsgXchange::vsgXchange)

install(TARGETS vsgpowerscaling RUNTIME DESTINATION bin)
