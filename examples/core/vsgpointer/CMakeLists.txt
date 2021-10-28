set(SOURCES vsgpointer.cpp)

add_executable(vsgpointer ${SOURCES})

target_link_libraries(vsgpointer vsg::vsg)

install(TARGETS vsgpointer RUNTIME DESTINATION bin)
