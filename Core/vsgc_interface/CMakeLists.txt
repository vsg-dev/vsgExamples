set(SOURCES vsgc_interface.c)

add_executable(vsgc_interface ${SOURCES})

target_link_libraries(vsgc_interface vsg)
