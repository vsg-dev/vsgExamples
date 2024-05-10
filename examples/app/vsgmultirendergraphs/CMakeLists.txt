set(SOURCES
    vsgmultirendergraphs.cpp
)

add_executable(vsgmultirendergraphs ${SOURCES})

target_link_libraries(vsgmultirendergraphs vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgmultirendergraphs PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgmultirendergraphs vsgXchange::vsgXchange)
endif()

install(TARGETS vsgmultirendergraphs RUNTIME DESTINATION bin)
