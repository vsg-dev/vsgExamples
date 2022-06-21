set(SOURCES vsglog.cpp
    Log.cpp)

add_executable(vsglog ${SOURCES})

target_link_libraries(vsglog vsg::vsg)

install(TARGETS vsglog RUNTIME DESTINATION bin)
