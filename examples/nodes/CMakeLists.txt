add_subdirectory(vsggroups)
add_subdirectory(vsglights)
add_subdirectory(vsgtransform)
add_subdirectory(vsgshadow)

if (vsgXchange_FOUND)
    add_subdirectory(vsgpagedlod)
    add_subdirectory(vsgtiledatabase)
endif()
