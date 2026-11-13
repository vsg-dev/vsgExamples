set(SOURCES vsgspotshadow.cpp)

add_executable(vsgspotshadow ${SOURCES})

target_link_libraries(vsgspotshadow vsg::vsg)

install(TARGETS vsgspotshadow RUNTIME DESTINATION bin)
