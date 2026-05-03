set(HEADERS DatabasePagerAutoscale.h)
set(SOURCES vsgpagedlodtest.cpp)

add_executable(vsgpagedlodtest ${HEADERS} ${SOURCES})

target_link_libraries(vsgpagedlodtest vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgpagedlodtest PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgpagedlodtest vsgXchange::vsgXchange)
endif()

install(TARGETS vsgpagedlodtest RUNTIME DESTINATION bin)
