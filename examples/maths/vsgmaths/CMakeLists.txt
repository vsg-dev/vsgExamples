set(SOURCES vsgmaths.cpp)

add_executable(vsgmaths ${SOURCES})
set_target_properties(vsgmaths PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgmaths vsg::vsg)

install(TARGETS vsgmaths RUNTIME DESTINATION bin)
