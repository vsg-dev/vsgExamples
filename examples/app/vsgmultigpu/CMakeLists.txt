set(SOURCES
    vsgmultigpu.cpp
)

add_executable(vsgmultigpu ${SOURCES})
set_target_properties(vsgmultigpu PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgmultigpu vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgmultigpu PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgmultigpu vsgXchange::vsgXchange)
endif()

install(TARGETS vsgmultigpu RUNTIME DESTINATION bin)
