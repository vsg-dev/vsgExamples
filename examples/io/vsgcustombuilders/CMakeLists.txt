set(SOURCES
    vsgcustombuilders.cpp
)

add_executable(vsgcustombuilders ${SOURCES})

target_compile_definitions(vsgcustombuilders PRIVATE vsgXchange_FOUND)
target_link_libraries(vsgcustombuilders vsgXchange::vsgXchange)

install(TARGETS vsgcustombuilders RUNTIME DESTINATION bin)
