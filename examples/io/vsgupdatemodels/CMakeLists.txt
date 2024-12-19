set(SOURCES vsgupdatemodels.cpp)

add_executable(vsgupdatemodels ${SOURCES})

target_link_libraries(vsgupdatemodels vsg::vsg)

install(TARGETS vsgupdatemodels RUNTIME DESTINATION bin)
