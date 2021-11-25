set(SOURCES
    vsgmultiwindows.cpp
)

add_executable(vsgmultiwindows ${SOURCES})

target_link_libraries(vsgmultiwindows vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgmultiwindows PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgmultiwindows vsgXchange::vsgXchange)
endif()

install(TARGETS vsgmultiwindows RUNTIME DESTINATION bin)
