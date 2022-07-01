project(vsgiosnative)
cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)


# Keep track of our own directory for future use (and default value of plist.in)
set(IOS_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})

# This little macro lets you set any Xcode specific property.
# This is from iOs CMake Toolchain
macro(ios_set_xcode_property TARGET XCODE_PROPERTY XCODE_VALUE XCODE_RELVERSION)
  set(XCODE_RELVERSION_I "${XCODE_RELVERSION}")
  if(XCODE_RELVERSION_I STREQUAL "All")
    set_property(TARGET ${TARGET} PROPERTY XCODE_ATTRIBUTE_${XCODE_PROPERTY} "${XCODE_VALUE}")
  else()
    set_property(TARGET ${TARGET} PROPERTY XCODE_ATTRIBUTE_${XCODE_PROPERTY}[variant=${XCODE_RELVERSION_I}] "${XCODE_VALUE}")
  endif()
endmacro() # ios_set_xcode_property

# We need that to parse arguments
include(CMakeParseArguments)

# Usage :
# add_ios_app(MyApp
#    NAME "My App"
#    BUNDLE_IDENTIFIER "com.company.app"
#    VERSION "1.2.3"
#    SHORT_VERSION "1.2.3"
#    LONG_VERSION "1.2.3.456"
#    CUSTOM_PLIST "path/to/Info.plist.in"
#    CODE_SIGN_IDENTITY "iPhone Developer"
#    TEAM_ID "AAAAAAAA"
#    PROVISIONING_PROFILE_SPECIFIER "profile-name"
#    COPYRIGHT "My Cool Copyright"
#    ASSET_DIR "path/to/Assets.xcassets"
#    MAIN_STORYBOARD "/path/to/Main.storyboard"
#        LAUNCHSCREEN_STORYBOARD "path/to/LaunchScreen.storyboard"
#        CATALOG_APPICON "AppIcon"
#        CATALOG_LAUNCHIMAGE "LaunchImage"
#    ORIENTATION_PORTRAIT
#    ORIENTATION_PORTRAIT_UPDOWN
#    ORIENTATION_LANDSCAPE_LEFT
#    ORIENTATION_LANDSCAPE_RIGHT
#    PHOTO_LIBRARY_USAGE_DESCRIPTION "My App needs to access your photo library."
#    SUPPORT_IPHONE
#    SUPPORT_IPAD
#    REQUIRES_FULL_SCREEN
#    HIDDEN_STATUS_BAR
#    IPA
#    UPLOAD_SYMBOL
#    DISTRIBUTION_METHOD "app-store"
#    VERBOSE
# )
function(add_ios_app TARGET)

    set(IOS_OPTIONS VERBOSE
        ORIENTATION_PORTRAIT
        ORIENTATION_PORTRAIT_UPDOWN
        ORIENTATION_LANDSCAPE_LEFT
        ORIENTATION_LANDSCAPE_RIGHT
        SUPPORT_IPHONE
        SUPPORT_IPAD
        REQUIRES_FULL_SCREEN
        HIDDEN_STATUS_BAR
        IPA
        UPLOAD_SYMBOL
        ITS_ENCRYPTION_EXPORT_COMPLIANCE_CODE
        )
    set(IOS_ONE_VALUE_ARG NAME
        BUNDLE_IDENTIFIER
        VERSION
        SHORT_VERSION
        LONG_VERSION
        CUSTOM_PLIST
        CODE_SIGN_IDENTITY
        TEAM_ID
        PROVISIONING_PROFILE_SPECIFIER
        COPYRIGHT
        ASSET_DIR
        LAUNCHSCREEN_STORYBOARD
        MAIN_STORYBOARD
        CATALOG_APPICON
        CATALOG_LAUNCHIMAGE
        DISTRIBUTION_METHOD
        PHOTO_LIBRARY_USAGE_DESCRIPTION
        )
    set(IOS_MULTI_VALUE_ARG
        RESOURCE_FILES
        )
     # parse the macro arguments
    cmake_parse_arguments(ARGIOS "${IOS_OPTIONS}" "${IOS_ONE_VALUE_ARG}" "${IOS_MULTI_VALUE_ARG}" ${ARGN})

    # Copy arg variables to local variables
    set(IOS_TARGET ${TARGET})
    set(IOS_NAME ${ARGIOS_NAME})
    set(IOS_BUNDLE_IDENTIFIER ${ARGIOS_BUNDLE_IDENTIFIER})
    set(IOS_VERSION ${ARGIOS_VERSION})
    set(IOS_SHORT_VERSION ${ARGIOS_SHORT_VERSION})
    set(IOS_LONG_VERSION ${ARGIOS_LONG_VERSION})
    set(IOS_CUSTOM_PLIST ${ARGIOS_CUSTOM_PLIST})
    if(NOT IOS_CODE_SIGN_IDENTITY)
        set(IOS_CODE_SIGN_IDENTITY ${ARGIOS_CODE_SIGN_IDENTITY})
    endif()
    if("${IOS_CODE_SIGN_IDENTITY}" STREQUAL "")
        set(IOS_CODE_SIGN_IDENTITY "iPhone Developer")
    endif()
    if(NOT IOS_TEAM_ID)
        set(IOS_TEAM_ID ${ARGIOS_TEAM_ID})
    endif()
    if(NOT IOS_PROVISIONING_PROFILE_SPECIFIER AND ARGIOS_PROVISIONING_PROFILE_SPECIFIER)
        set(IOS_PROVISIONING_PROFILE_SPECIFIER ${ARGIOS_PROVISIONING_PROFILE_SPECIFIER})
    endif()
    set(IOS_COPYRIGHT ${ARGIOS_COPYRIGHT})
    set(IOS_ASSET_DIR ${ARGIOS_ASSET_DIR})
    set(IOS_LAUNCHSCREEN_STORYBOARD ${ARGIOS_LAUNCHSCREEN_STORYBOARD})
    set(IOS_MAIN_STORYBOARD ${ARGIOS_MAIN_STORYBOARD})
    set(IOS_CATALOG_APPICON ${ARGIOS_CATALOG_APPICON})
    set(IOS_CATALOG_LAUNCHIMAGE ${ARGIOS_CATALOG_LAUNCHIMAGE})

    set(IOS_RESOURCE_FILES "${ARGIOS_RESOURCE_FILES}")

    set(IOS_ORIENTATION_PORTRAIT ${ARGIOS_ORIENTATION_PORTRAIT})
    set(IOS_ORIENTATION_PORTRAIT_UPDOWN ${ARGIOS_ORIENTATION_PORTRAIT_UPDOWN})
    set(IOS_ORIENTATION_LANDSCAPE_LEFT ${ARGIOS_ORIENTATION_LANDSCAPE_LEFT})
    set(IOS_ORIENTATION_LANDSCAPE_RIGHT ${ARGIOS_ORIENTATION_LANDSCAPE_RIGHT})
    set(IOS_SUPPORT_IPHONE ${ARGIOS_SUPPORT_IPHONE})
    set(IOS_SUPPORT_IPAD ${ARGIOS_SUPPORT_IPAD})
    set(IOS_REQUIRES_FULL_SCREEN ${ARGIOS_REQUIRES_FULL_SCREEN})
    set(IOS_HIDDEN_STATUS_BAR ${ARGIOS_HIDDEN_STATUS_BAR})
    set(IOS_PHOTO_LIBRARY_USAGE_DESCRIPTION ${ARGIOS_PHOTO_LIBRARY_USAGE_DESCRIPTION})

    set(IOS_IPA ${ARGIOS_IPA})
    set(IOS_UPLOAD_SYMBOL ${ARGIOS_UPLOAD_SYMBOL})
    if(NOT IOS_DISTRIBUTION_METHOD)
        set(IOS_DISTRIBUTION_METHOD ${ARGIOS_DISTRIBUTION_METHOD})
    endif()
    if("${IOS_DISTRIBUTION_METHOD}" STREQUAL "")
        set(IOS_DISTRIBUTION_METHOD "app-store")
    endif()

    # Allow user to override IOS_ITS_ENCRYPTION_EXPORT_COMPLIANCE_CODE from cache/command line
    if(NOT IOS_ITS_ENCRYPTION_EXPORT_COMPLIANCE_CODE)
        set(IOS_ITS_ENCRYPTION_EXPORT_COMPLIANCE_CODE ${ARGIOS_ITS_ENCRYPTION_EXPORT_COMPLIANCE_CODE})
    endif()
    # IOS_ITS_ENCRYPTION_KEYS is used in Info.plist.in
    if(IOS_ITS_ENCRYPTION_EXPORT_COMPLIANCE_CODE)
        set(IOS_ITS_ENCRYPTION_KEYS "<key>ITSAppUsesNonExemptEncryption</key><true/>\n    <key>ITSEncryptionExportComplianceCode</key>\n    <string>${IOS_ITS_ENCRYPTION_EXPORT_COMPLIANCE_CODE}</string>" PARENT_SCOPE)
    else()
        set(IOS_ITS_ENCRYPTION_KEYS "<key>ITSAppUsesNonExemptEncryption</key><false/>" PARENT_SCOPE)
    endif()

    set(IOS_VERBOSE ${ARGIOS_VERBOSE})

    if(NOT ARGIOS_NAME)
        if(IOS_VERBOSE)
            message(STATUS "NAME not provided when calling add_ios_app. Name will be default to ${IOS_TARGET}")
        endif()
        set(IOS_NAME ${IOS_TARGET})
    endif()

    # Warning if no default BUNDLE_IDENTIFIER is set
    if(NOT ARGIOS_BUNDLE_IDENTIFIER)
        if(IOS_VERBOSE)
            message(STATUS "BUNDLE_IDENTIFIER not set when calling add_ios_app. "
                "You can fix this by hand in XCode. "
                "The BUNDLE_IDENTIFIER is defaulted to ${CMAKE_PROJECT_NAME}")
        endif() # IOS_VERBOSE
        set(IOS_BUNDLE_IDENTIFIER ${CMAKE_PROJECT_NAME}.${IOS_TARGET})
    endif() # NOT ARGIOS_BUNDLE_IDENTIFIER

    # Warning if no version
    if(NOT ARGIOS_VERSION)
        set(IOS_VERSION ${CMAKE_PROJECT_VERSION})
        if("${IOS_VERSION}" STREQUAL "")
            set(IOS_VERSION "1.0.0")
        endif()
        if(IOS_VERBOSE)
            message(STATUS "VERSION not set when calling add_ios_app. "
                "Default VERSION to ${IOS_VERSION}")
        endif() # IOS_VERBOSE
    endif() # NOT ARGIOS_VERSION

    # Default value for SHORT_VERSION
    if(NOT IOS_SHORT_VERSION)
        if(IOS_VERBOSE)
            message(STATUS "SHORT_VERSION not specified, default to ${IOS_VERSION}")
        endif() # IOS_VERBOSE
        set(IOS_SHORT_VERSION ${IOS_VERSION})
    endif() # NOT IOS_SHORT_VERSION

    # Default value for long version
    if(NOT IOS_LONG_VERSION)
        if(IOS_VERBOSE)
            message(STATUS "LONG_VERSION not specified, default to ${IOS_VERSION}")
        endif() # IOS_VERBOSE
        set(IOS_LONG_VERSION ${IOS_VERSION})
    endif() # NOT IOS_LONG_VERSION

    # Default value for plist file
    if(NOT IOS_CUSTOM_PLIST)
        set(IOS_CUSTOM_PLIST ${IOS_SOURCE_DIR}/templates/Info.plist.in)
        if(IOS_VERBOSE)
            message(STATUS "CUSTOM_PLIST not specified, default to ${IOS_CUSTOM_PLIST}")
        endif() # IOS_VERBOSE
    endif() # NOT IOS_CUSTOM_PLIST

    if(NOT IOS_CATALOG_APPICON)
        set(IOS_CATALOG_APPICON "AppIcon")
        if(IOS_VERBOSE)
            message(STATUS "CATALOG_APPICON not specified, default to ${IOS_CATALOG_APPICON}.")
        endif() # IOS_VERBOSE
    endif() # NOT IOS_CATALOG_APPICON

    # Print macro configuration
    if(IOS_VERBOSE)
        message(STATUS "------ IosCMake Configuration ------")
        message(STATUS "TARGET                              : ${IOS_TARGET}")
        message(STATUS "NAME                                : ${IOS_NAME}")
        message(STATUS "BUNDLE_IDENTIFIER                   : ${IOS_BUNDLE_IDENTIFIER}")
        message(STATUS "VERSION                             : ${IOS_VERSION}")
        message(STATUS "SHORT_VERSION                       : ${IOS_SHORT_VERSION}")
        message(STATUS "LONG_VERSION                        : ${IOS_LONG_VERSION}")
        message(STATUS "CUSTOM_PLIST                        : ${IOS_CUSTOM_PLIST}")
        message(STATUS "CODE_SIGN_IDENTITY                  : ${IOS_CODE_SIGN_IDENTITY}")
        message(STATUS "TEAM_ID                             : ${IOS_TEAM_ID}")
        if(IOS_PROVISIONING_PROFILE_SPECIFIER)
            message(STATUS "PROVISIONING_PROFILE_SPECIFIER      : ${IOS_PROVISIONING_PROFILE_SPECIFIER}")
        endif()
        message(STATUS "COPYRIGHT                           : ${IOS_COPYRIGHT}")
        message(STATUS "ASSET_DIR                           : ${IOS_ASSET_DIR}")
        message(STATUS "CATALOG_APPICON                     : ${IOS_CATALOG_APPICON}")
        message(STATUS "CATALOG_LAUNCHIMAGE                 : ${IOS_CATALOG_LAUNCHIMAGE}")
        message(STATUS "LAUNCHSCREEN_STORYBOARD             : ${IOS_LAUNCHSCREEN_STORYBOARD}")
        message(STATUS "MAIN_STORYBOARD                     : ${IOS_MAIN_STORYBOARD}")
        message(STATUS "ORIENTATION_PORTRAIT                : ${IOS_ORIENTATION_PORTRAIT}")
        message(STATUS "ORIENTATION_PORTRAIT_UPDOWN         : ${IOS_ORIENTATION_PORTRAIT_UPDOWN}")
        message(STATUS "ORIENTATION_LANDSCAPE_LEFT          : ${IOS_ORIENTATION_LANDSCAPE_LEFT}")
        message(STATUS "ORIENTATION_LANDSCAPE_RIGHT         : ${IOS_ORIENTATION_LANDSCAPE_RIGHT}")
        if(IOS_PHOTO_LIBRARY_USAGE_DESCRIPTION)
            message(STATUS "PHOTO_LIBRARY_USAGE_DESCRIPTION     : ${IOS_PHOTO_LIBRARY_USAGE_DESCRIPTION}")
        endif()
        message(STATUS "SUPPORT_IPHONE                      : ${IOS_SUPPORT_IPHONE}")
        message(STATUS "SUPPORT_IPAD                        : ${IOS_SUPPORT_IPAD}")
        message(STATUS "REQUIRES_FULL_SCREEN                : ${IOS_REQUIRES_FULL_SCREEN}")
        message(STATUS "HIDDEN_STATUS_BAR                   : ${IOS_HIDDEN_STATUS_BAR}")
        message(STATUS "IPA                                 : ${IOS_IPA}")
        message(STATUS "UPLOAD_SYMBOL                       : ${IOS_UPLOAD_SYMBOL}")
        message(STATUS "DISTRIBUTION_METHOD                 : ${IOS_DISTRIBUTION_METHOD}")
        message(STATUS "RESOURCE_FILES                      : ${IOS_RESOURCE_FILES}")
        message(STATUS "------ IosCMake END Configuration ------")
    endif() # IOS_VERBOSE

    # Bundle executable.
    if(IOS_VERBOSE)
        message(STATUS "Set property MACOSX_BUNDLE to ${IOS_TARGET}")
    endif() # IOS_VERBOSE
    set_target_properties(${IOS_TARGET} PROPERTIES MACOSX_BUNDLE ON)

    # Remove Mess
    function(ios_clean_paths)
        foreach(_path_group ${ARGN})
            foreach(_path ${${_path_group}})
                get_filename_component(_path_cleaned ${_path} REALPATH)
                file(TO_NATIVE_PATH ${_path_cleaned} _path_cleaned)
                set(_path_group_cleaned ${_path_group_cleaned} ${_path_cleaned})
            endforeach()
        set(${_path_group} ${_path_group_cleaned} PARENT_SCOPE)
        endforeach()
    endfunction()

    macro(ios_handle_cyclical_linking LIBS)
        if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT APPLE))
            set(${LIBS} -Wl,--start-group ${${LIBS}} -Wl,--end-group)
        endif()
    endmacro()

    # let's not be picky, just throw all the available static libraries at the linker and let it figure out which ones are actually needed
    # a 'foreach' is used because 'target_link_libraries' doesn't handle lists correctly (the ; messes it up and nothing actually gets linked against)
    if(IOS_TARGET_IS_WINDOWS)
        set(_DEBUG_SUFFIX d)
    elseif(IOS_TARGET_IS_IOS OR IOS_TARGET_IS_MAC)
        set(_DEBUG_SUFFIX _debug)
    else()
        set(_DEBUG_SUFFIX)
    endif()

    set(_LIBS_BASE_DIR "${IOS_ROOT}/lib")
    ios_clean_paths(_LIBS_BASE_DIR)
    file(GLOB_RECURSE _LIBS "${_LIBS_BASE_DIR}/*${CMAKE_STATIC_LIBRARY_SUFFIX}")
    foreach(_LIB ${_LIBS})
        string(REGEX MATCH ".*${_DEBUG_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}$" _IS_DEBUG_LIB ${_LIB})
        string(REGEX MATCH ".*_iphonesimulator${_DEBUG_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}$" _IS_DEBUG_SIM_LIB ${_LIB})
        string(REGEX MATCH ".*_iphonesimulator${CMAKE_STATIC_LIBRARY_SUFFIX}$" _IS_SIM_LIB ${_LIB})

        if(NOT _IS_BOOTSTRAP AND NOT _IS_DEVTOOLS AND NOT _IS_DEBUG_SIM_LIB AND NOT _IS_SIM_LIB)
            if(_IS_DEBUG_LIB OR NOT _DEBUG_SUFFIX)
                set(EXTRA_LIBS ${EXTRA_LIBS} debug "${_LIB}")
            endif()
            if(NOT _IS_DEBUG_LIB OR NOT _DEBUG_SUFFIX)
                set(EXTRA_LIBS ${EXTRA_LIBS} optimized "${_LIB}")
            endif()
        endif()
    endforeach()

    set(_PLUGINS_BASE_DIR "${IOS_ROOT}/plugins")
    ios_clean_paths(_PLUGINS_BASE_DIR)
    file(GLOB_RECURSE _PLUGINS "${_PLUGINS_BASE_DIR}/*${CMAKE_STATIC_LIBRARY_SUFFIX}")
    foreach(_PLUGIN ${_PLUGINS})
        string(REGEX MATCH ".*${_DEBUG_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}$" _IS_DEBUG_LIB ${_PLUGIN})
        string(REGEX MATCH ".*_iphonesimulator${_DEBUG_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}$" _IS_DEBUG_SIM_LIB ${_PLUGIN})
        string(REGEX MATCH ".*_iphonesimulator${CMAKE_STATIC_LIBRARY_SUFFIX}$" _IS_SIM_LIB ${_PLUGIN})

        if(NOT _IS_DEBUG_SIM_LIB AND NOT _IS_SIM_LIB)
            if(_IS_DEBUG_LIB OR NOT _DEBUG_SUFFIX)
                set(EXTRA_LIBS ${EXTRA_LIBS} debug "${_PLUGIN}")
            endif()
            if(NOT _IS_DEBUG_LIB OR NOT _DEBUG_SUFFIX)
                set(EXTRA_LIBS ${EXTRA_LIBS} optimized "${_PLUGIN}")
            endif()
        endif()
    endforeach()

    # static linking
    set(LIBRARIES ${LIBRARIES} ${EXTRA_LIBS})
    ios_handle_cyclical_linking(LIBRARIES)

    # Define entry point for correct initialization.
    # Maybe set this optional in the future if user wants to set his own entry point ?
    if(IOS_VERBOSE)
        message(STATUS "Add -e _main_wrapper linker flag to ${IOS_TARGET} to change application entry point to create UIApplication before QApplication")
    endif() # IOS_VERBOSE
   # target_link_libraries(${IOS_TARGET} PUBLIC ${LIBRARIES} "-e _main_wrapper")

    # Set XCode property for automatic code sign
    if(IOS_CODE_SIGN_IDENTITY)
        ios_set_xcode_property(${IOS_TARGET} CODE_SIGN_IDENTITY ${IOS_CODE_SIGN_IDENTITY} "All")
    endif()
    if(IOS_TEAM_ID)
        ios_set_xcode_property(${IOS_TARGET} DEVELOPMENT_TEAM ${IOS_TEAM_ID} "All")
        set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ${IOS_TEAM_ID} CACHE INTERNAL "" FORCE)
    endif()
    if(IOS_PROVISIONING_PROFILE_SPECIFIER)
        ios_set_xcode_property(${IOS_TARGET} PROVISIONING_PROFILE_SPECIFIER ${IOS_PROVISIONING_PROFILE_SPECIFIER} "All")
    endif()

    # Ugly but working
    if(IOS_SUPPORT_IPAD AND IOS_SUPPORT_IPHONE)
        ios_set_xcode_property(${IOS_TARGET} TARGETED_DEVICE_FAMILY "1,2" "All")
    elseif(IOS_SUPPORT_IPAD)
        ios_set_xcode_property(${IOS_TARGET} TARGETED_DEVICE_FAMILY "2" "All")
    elseif(IOS_SUPPORT_IPHONE)
        ios_set_xcode_property(${IOS_TARGET} TARGETED_DEVICE_FAMILY "1" "All")
    else()
        if(IOS_VERBOSE)
            message(STATUS "None of SUPPORT_IPHONE or SUPPORT_IPAD are set. Support both by default.")
        endif()
        ios_set_xcode_property(${IOS_TARGET} TARGETED_DEVICE_FAMILY "1,2" "All")
    endif()

    # Set AppIcon Catalog
    if(IOS_CATALOG_APPICON)
        ios_set_xcode_property (${IOS_TARGET} ASSETCATALOG_COMPILER_APPICON_NAME ${IOS_CATALOG_APPICON} "All")
    endif()

    # Set LaunchImage Catalog
    if(IOS_CATALOG_LAUNCHIMAGE)
        ios_set_xcode_property (${IOS_TARGET} ASSETCATALOG_COMPILER_LAUNCHIMAGE_NAME ${IOS_CATALOG_LAUNCHIMAGE} "All")
    endif()
    # Make sure a publish dialog is set in XCode.
    # If INSTALL_PATH is empty it won't be possible to deploy to App Store
    ios_set_xcode_property(${IOS_TARGET} INSTALL_PATH "/Applications" "All")

    # Set CMake variables for plist
    set(MACOSX_BUNDLE_EXECUTABLE_NAME ${IOS_NAME} PARENT_SCOPE)
    set(MACOSX_BUNDLE_INFO_STRING ${IOS_NAME} PARENT_SCOPE)
    set(MACOSX_BUNDLE_GUI_IDENTIFIER ${IOS_BUNDLE_IDENTIFIER} PARENT_SCOPE)
    set(MACOSX_BUNDLE_BUNDLE_NAME ${IOS_NAME} PARENT_SCOPE)
    #set(MACOSX_BUNDLE_ICON_FILE "${PROJECT_SOURCE_DIR} PARENT_SCOPE/platform/ios/Assets.xcassets/AppIcon.appiconset")
    set(MACOSX_BUNDLE_BUNDLE_VERSION ${IOS_VERSION} PARENT_SCOPE)
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${IOS_SHORT_VERSION} PARENT_SCOPE)
    set(MACOSX_BUNDLE_LONG_VERSION_STRING ${IOS_LONG_VERSION} PARENT_SCOPE)
    set(MACOSX_BUNDLE_COPYRIGHT ${IOS_COPYRIGHT} PARENT_SCOPE)

    # Set require full screen
    if(IOS_REQUIRES_FULL_SCREEN)
        set(MACOSX_BUNDLE_REQUIRES_FULL_SCREEN "YES" PARENT_SCOPE)
        if(IOS_VERBOSE)
            message(STATUS "Add UIRequiresFullScreen flag to Info.pList")
        endif() # IOS_VERBOSE
    else() # IOS_REQUIRES_FULL_SCREEN
        set(MACOSX_BUNDLE_REQUIRES_FULL_SCREEN "NO" PARENT_SCOPE)
    endif() # IOS_REQUIRES_FULL_SCREEN

    # Set hidden status bar
    if(IOS_HIDDEN_STATUS_BAR)
        set(MACOSX_BUNDLE_HIDDEN_STATUS_BAR "true" PARENT_SCOPE)
        if(IOS_VERBOSE)
            message(STATUS "Add UIStatusBarHidden flag to Info.pList")
        endif() # IOS_VERBOSE
    else() # IOS_HIDDEN_STATUS_BAR
        set(MACOSX_BUNDLE_HIDDEN_STATUS_BAR "false" PARENT_SCOPE)
    endif() # IOS_HIDDEN_STATUS_BAR

    # Add orientation flags
    if(IOS_ORIENTATION_PORTRAIT)
        set(MACOSX_BUNDLE_PORTRAIT "UIInterfaceOrientationPortrait" PARENT_SCOPE)
        if(IOS_VERBOSE)
            message(STATUS "Add UIInterfaceOrientationPortrait flag to Info.pList")
        endif() # IOS_VERBOSE
    endif()
    if(IOS_ORIENTATION_PORTRAIT_UPDOWN)
        set(MACOSX_BUNDLE_PORTRAITUPDOWN "UIInterfaceOrientationPortraitUpsideDown" PARENT_SCOPE)
        if(IOS_VERBOSE)
            message(STATUS "Add UIInterfaceOrientationPortraitUpsideDown flag to Info.pList")
        endif() # IOS_VERBOSE
    endif()
    if(IOS_ORIENTATION_LANDSCAPE_LEFT)
        set(MACOSX_BUNDLE_LANDSCAPELEFT "UIInterfaceOrientationLandscapeLeft" PARENT_SCOPE)
        if(IOS_VERBOSE)
            message(STATUS "Add UIInterfaceOrientationLandscapeLeft flag to Info.pList")
        endif() # IOS_VERBOSE
    endif()
    if(IOS_ORIENTATION_LANDSCAPE_RIGHT)
        set(MACOSX_BUNDLE_LANDSCAPERIGHT "UIInterfaceOrientationLandscapeRight" PARENT_SCOPE)
        if(IOS_VERBOSE)
            message(STATUS "Add UIInterfaceOrientationLandscapeRight flag to Info.pList")
        endif() # IOS_VERBOSE
    endif()
    if(NOT IOS_ORIENTATION_PORTRAIT AND
       NOT IOS_ORIENTATION_PORTRAIT_UPDOWN AND
       NOT IOS_ORIENTATION_LANDSCAPE_LEFT AND
       NOT IOS_ORIENTATION_LANDSCAPE_RIGHT)
        set(MACOSX_BUNDLE_PORTRAIT "UIInterfaceOrientationPortrait" PARENT_SCOPE)
        if(IOS_VERBOSE)
            message(STATUS "Add UIInterfaceOrientationPortrait as default to Info.pList becayse nothing was set by user in the list"
            "{ORIENTATION_PORTRAIT, ORIENTATION_PORTRAIT_UPDOWN, ORIENTATION_LANDSCAPE_LEFT, ORIENTATION_LANDSCAPE_RIGHT}")
        endif() # IOS_VERBOSE
    endif()

    # Set Custom pList
    set_target_properties(${IOS_TARGET} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${IOS_CUSTOM_PLIST})

    # Add asset dir as resource
    if(IOS_ASSET_DIR)
        target_sources(${IOS_TARGET} PRIVATE ${IOS_ASSET_DIR})
        set_source_files_properties(${IOS_ASSET_DIR} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    else(IOS_ASSET_DIR)
        if(IOS_VERBOSE)
            message(STATUS "No Asset dir specified. This is the recommended way to add Icons and LaunchImage")
        endif() # IOS_VERBOSE
    endif() # IOS_ASSET_DIR

    foreach(_resource IN LISTS IOS_RESOURCE_FILES)
        if(IOS_VERBOSE)
            message(STATUS "Add resource: ${_resource}")
        endif()
        target_sources(${IOS_TARGET} PRIVATE ${_resource})
        set_source_files_properties(${_resource} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    endforeach()


    # Add Launchscreen storyboard as resource
    if(IOS_LAUNCHSCREEN_STORYBOARD)
        target_sources(${IOS_TARGET} PRIVATE ${IOS_LAUNCHSCREEN_STORYBOARD})
        set_source_files_properties(${IOS_LAUNCHSCREEN_STORYBOARD} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        get_filename_component(STORYBOARD_FILE_NAME ${IOS_LAUNCHSCREEN_STORYBOARD} NAME_WE)
        if(IOS_VERBOSE)
            message(STATUS "Add UILaunchStoryboardName key with value ${STORYBOARD_FILE_NAME} in Info.pList")
        endif() # IOS_VERBOSE
        set(MACOSX_BUNDLE_LAUNCHSCREEN_STORYBOARD ${STORYBOARD_FILE_NAME} PARENT_SCOPE)
    else() # IOS_LAUNCHSCREEN_STORYBOARD
        if(IOS_VERBOSE)
            if(NOT IOS_REQUIRES_FULL_SCREEN)
            message(WARNING "LaunchScreen.storyboard isn't specified, it's is now recommended to have one if you don't set REQUIRES_FULL_SCREEN")
            endif() # NOT IOS_REQUIRES_FULL_SCREEN
        endif() # IOS_VERBOSE
    endif() # IOS_LAUNCHSCREEN_STORYBOARD

    # Add Main storyboard as resource
    if(IOS_MAIN_STORYBOARD)
        target_sources(${IOS_TARGET} PRIVATE ${IOS_MAIN_STORYBOARD})
        set_source_files_properties(${IOS_MAIN_STORYBOARD} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        # We just need to set the filename without extension
        string(REGEX MATCH "([a-zA-Z0-9 \\-_]*)\\.storyboard" STORYBOARD_FILE_NAME ${IOS_MAIN_STORYBOARD})
        if(IOS_VERBOSE)
            message(STATUS "Add UIMainStoryboardFile key with value ${CMAKE_MATCH_1} in Info.pList")
        endif() # IOS_VERBOSE
        set(MACOSX_BUNDLE_MAIN_STORYBOARD ${CMAKE_MATCH_1} PARENT_SCOPE)
    endif(IOS_MAIN_STORYBOARD)

    if(${PLATFORM_INT} MATCHES ".*SIMULATOR.*" AND IOS_IPA)
        unset(IOS_IPA)
        message(WARNING "Ipa can't be enabled for simulator.")
    endif()

    if(IOS_IPA)

        set(IOS_TARGET_ARCHIVE ${IOS_TARGET}Archive)
        set(IOS_TARGET_IPA ${IOS_TARGET}Ipa)

        set(IOS_TARGET_ARCHIVE_PATH ${CMAKE_CURRENT_BINARY_DIR}/${IOS_TARGET}.xcarchive)
        set(IOS_TARGET_IPA_PATH ${CMAKE_CURRENT_BINARY_DIR}/${IOS_TARGET_IPA})

        # Generate archive
        add_custom_target(${IOS_TARGET_ARCHIVE}
          ALL
          DEPENDS ${IOS_TARGET}
          COMMAND xcodebuild
            -project ${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.xcodeproj
            -scheme ${IOS_TARGET}
            -archivePath ${IOS_TARGET_ARCHIVE_PATH}
            archive
        )

        # Generate IPA
        if(IOS_PROVISIONING_PROFILE_SPECIFIER)
            set(IOS_EXPORT_SIGNING_TYPE "manual")
        else()
            set(IOS_EXPORT_SIGNING_TYPE "automatic")
        endif()

        set(IOS_PROVISIONING_PROFILES_KEY
            "<key>provisioningProfiles</key>\n    <dict>\n        <key>${IOS_BUNDLE_IDENTIFIER}</key>\n        <string>${IOS_PROVISIONING_PROFILE_SPECIFIER}</string>\n     </dict>\n"
        )

        if(ENABLE_BITCODE)
            set(IOS_ENABLE_BITCODE TRUE)
        endif()

        if(IOS_ENABLE_BITCODE)
            set(IOS_ENABLE_BITCODE_KEY "<key>compileBitcode</key><true/>")
        else()
            set(IOS_ENABLE_BITCODE_KEY "")
        endif()

        if(IOS_UPLOAD_SYMBOL)
            set(IOS_UPLOAD_SYMBOL_KEY "<key>uploadSymbols</key><true/>")
        else()
            set(IOS_UPLOAD_SYMBOL_KEY "")
        endif()

        if(IOS_PHOTO_LIBRARY_USAGE_DESCRIPTION)
            set(MACOSX_BUNDLE_PHOTO_LIBRARY_USAGE_DESCRIPTION "<key>NSPhotoLibraryUsageDescription</key> <string>${IOS_PHOTO_LIBRARY_USAGE_DESCRIPTION}</string>" PARENT_SCOPE)
        endif()

        set(IOS_EXPORT_OPTIONS_FILE ${CMAKE_CURRENT_BINARY_DIR}/${IOS_TARGET}ExportOptions.plist)
        configure_file(${IOS_SOURCE_DIR}/templates/ExportOptions.plist.in ${IOS_EXPORT_OPTIONS_FILE})

        add_custom_target(${IOS_TARGET_IPA}
          ALL
          DEPENDS ${IOS_TARGET_ARCHIVE}
          COMMAND xcodebuild -exportArchive
          -archivePath ${IOS_TARGET_ARCHIVE_PATH}
          -exportOptionsPlist ${IOS_EXPORT_OPTIONS_FILE}
          -exportPath ${IOS_TARGET_IPA_PATH}
          ${IOS_EXPORT_ARCHIVE_XCODEBUILD_FLAGS}
        )

    endif()

endfunction()



set(TARGET "vsgiosnative")

set(SOURCES
    main.mm
)

add_executable(
    ${TARGET}
    ${SOURCES}
    ${INCLUDES}
)

set_property(TARGET ${TARGET} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${TARGET} PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET ${TARGET} PROPERTY CXX_STANDARD 17)

find_library(UIKIT_LIBRARY UiKit REQUIRED)
find_library(METAL_LIBRARY Metal REQUIRED)
find_library(QUARTZCORE_LIBRARY QuartzCore REQUIRED)
set(COREGRAPHICS_LIBRARY /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS14.5.sdk/System/Library/Frameworks/CoreGraphics.framework) # TODO make this a findcmake module
find_library(COREGRAPHICS_LIBRARY CoreGrahics REQUIRED)
string(REPLACE "QuartzCore" "CoreGraphics" COREGRAPHICS_LIBRARY "${COREGRAPHICS_LIBRARY}" ) # patch because cmake wrongly points to QuartzCore here!

find_package(vsg 0.2.10 REQUIRED)

set(LIBRARIES
            ${LIBRARIES}
            PRIVATE ${UIKIT_LIBRARY}
            PRIVATE ${QUARTZCORE_LIBRARY}
            PRIVATE ${METAL_LIBRARY}
            PRIVATE ${COREGRAPHICS_LIBRARY}
            PRIVATE ${Vulkan_LIBRARY}
    )


target_link_libraries(${TARGET}
    PUBLIC
        vsg::vsg
        ${LIBRARIES}
)

 add_ios_app(${TARGET}
            NAME ${TARGET}
            BUNDLE_IDENTIFIER "com.iconiqlabs.${TARGET}"
            VERSION "1.2.3"
            SHORT_VERSION "1.2.3"
            LONG_VERSION "1.2.3.456"
            # CUSTOM_PLIST "path/to/MacOSXBundleInfo.plist.in"
            CODE_SIGN_IDENTITY "7139E00C7117F491459E9590A790AAFACAD36EA3"
            TEAM_ID "AFNNR55AVS"
            # PROVISIONING_PROFILE_SPECIFIER "my-profile"
            COPYRIGHT "My Cool Copyright"
            # ASSET_DIR "path/to/Assets.xcassets"
            # MAIN_STORYBOARD "Main.storyboard"
            # LAUNCHSCREEN_STORYBOARD "path/to/LaunchScreen.storyboard"
            # CATALOG_APPICON "AppIcon"
            # CATALOG_LAUNCHIMAGE "LaunchImage"
            ORIENTATION_PORTRAIT
            ORIENTATION_PORTRAIT_UPDOWN
            ORIENTATION_LANDSCAPE_LEFT
            ORIENTATION_LANDSCAPE_RIGHT
            # PHOTO_LIBRARY_USAGE_DESCRIPTION "My App needs to access your photo library."
            SUPPORT_IPHONE
            SUPPORT_IPAD
            REQUIRES_FULL_SCREEN
            HIDDEN_STATUS_BAR
            IPA
            UPLOAD_SYMBOL
            VERBOSE
       )

