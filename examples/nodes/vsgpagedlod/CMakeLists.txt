set(SOURCES
    ../../shared/AnimationPath.cpp
    TileReader.h
    TileReader.cpp
    vsgpagedlod.cpp
)

add_executable(vsgpagedlod ${SOURCES})

target_link_libraries(vsgpagedlod vsg::vsg)

target_compile_definitions(vsgpagedlod PRIVATE vsgXchange_FOUND)
target_link_libraries(vsgpagedlod vsgXchange::vsgXchange)

install(TARGETS vsgpagedlod RUNTIME DESTINATION bin)
