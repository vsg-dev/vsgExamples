set(SOURCES vsginput.cpp Text.cpp Text.h)

add_executable(vsginput ${SOURCES})

target_link_libraries(vsginput vsg)
