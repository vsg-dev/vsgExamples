set(SOURCES
    vsgdrawindirect.cpp
)

add_executable(vsgdrawindirect ${SOURCES})

target_link_libraries(vsgdrawindirect vsg::vsg)

install(TARGETS vsgdrawindirect RUNTIME DESTINATION bin)
