set(SOURCES vsgmaths.cpp)

add_executable(vsgmaths ${SOURCES})

target_link_libraries(vsgmaths vsg::vsg)

install(TARGETS vsgmaths RUNTIME DESTINATION bin)
