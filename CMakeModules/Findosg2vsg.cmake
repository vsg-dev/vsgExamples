# FindOSG2VSG,cmake sourced from the GL_vs_VK project, (C) Copyright (c) 2017 Damian Dyńdo
#
# This module is taken from CMake original find-modules and adapted for the
# needs of this project:
# Below notice is from original file:
#
# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindOSG2VSG
# ----------
#
# Try to find OSG2VSG
#
# IMPORTED Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines :prop_tgt:`IMPORTED` target ``OSG2VSG::OSG2VSG``, if
# OSG2VSG has been found.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables::
#
#   OSG2VSG_FOUND           - True if OSG2VSG was found
#   OSG2VSG_INCLUDE_DIRS    - include directories for OSG2VSG and third-party OSG2VSG-Hpp
#   OSG2VSG_LIBRARIES       - link against this library to use OSG2VSG
#
# The module will also define the following cache variables::
#
#   OSG2VSG_INCLUDE_DIR     - the OSG2VSG include directory
#   OSG2VSG_LIBRARY         - the path to the OSG2VSG library
#

# OSG2VSG header file
find_path(OSG2VSG_INCLUDE_DIR
  NAMES osg2vsg/Export.h
  PATHS
    ${OSG2VSG_DIR}/include
    "$ENV{OSG2VSG_DIR}/include"
  NO_DEFAULT_PATH
  )

find_library(OSG2VSG_LIBRARY_RELEASE
    NAMES osg2vsg osg2vsgs
    PATHS
    ${OSG2VSG_DIR}/lib
    "$ENV{OSG2VSG_DIR}/lib")

find_library(OSG2VSG_LIBRARY_DEBUG
    NAMES osg2vsgd osg2vsgrd
    PATHS
    ${OSG2VSG_DIR}/lib
    "$ENV{OSG2VSG_DIR}/lib")

set(OSG2VSG_LIBRARIES ${OSG2VSG_LIBRARY})
set(OSG2VSG_INCLUDE_DIRS ${OSG2VSG_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OSG2VSG
  DEFAULT_MSG
  OSG2VSG_LIBRARY OSG2VSG_INCLUDE_DIR)

# mark_as_advanced(OSG2VSG_INCLUDE_DIR OSG2VSG_LIBRARY)

if(OSG2VSG_FOUND AND NOT TARGET OSG2VSG::OSG2VSG)

  add_library(OSG2VSG::OSG2VSG UNKNOWN IMPORTED)
  set_target_properties(OSG2VSG::OSG2VSG PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${OSG2VSG_INCLUDE_DIR}")

  if (OSG2VSG_LIBRARY_RELEASE)
    set_property(TARGET OSG2VSG::OSG2VSG APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
    set_target_properties(OSG2VSG::OSG2VSG PROPERTIES IMPORTED_LOCATION_RELEASE "${OSG2VSG_LIBRARY_RELEASE}")
  endif()

  if (OSG2VSG_LIBRARY_DEBUG)
    set_property(TARGET OSG2VSG::OSG2VSG APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
    set_target_properties(OSG2VSG::OSG2VSG PROPERTIES IMPORTED_LOCATION_DEBUG "${OSG2VSG_LIBRARY_DEBUG}")
  endif()
endif()
