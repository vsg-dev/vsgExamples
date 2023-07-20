add_subdirectory(vsggroups)
add_subdirectory(vsglights)
add_subdirectory(vsgtransform)

if (vsgXchange_FOUND)
    add_subdirectory(vsgpagedlod)
    add_subdirectory(vsgtiledatabase)
endif()
