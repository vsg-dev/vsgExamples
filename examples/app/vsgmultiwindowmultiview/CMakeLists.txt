set(SOURCES
    vsgmultiwindowmultiview.cpp
)

add_executable(vsgmultiwindowmultiview ${SOURCES})

target_link_libraries(vsgmultiwindowmultiview vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgmultiwindowmultiview PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgmultiwindowmultiview vsgXchange::vsgXchange)
endif()

install(TARGETS vsgmultiwindowmultiview RUNTIME DESTINATION bin)
