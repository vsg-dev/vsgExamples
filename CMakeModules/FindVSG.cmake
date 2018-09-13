# FindVSG,cmake sourced from the GL_vs_VK project, (C) Copyright (c) 2017 Damian Dyńdo
#
# This module is taken from CMake original find-modules and adapted for the
# needs of this project:
# Below notice is from original file:
#
# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindVSG
# ----------
#
# Try to find VSG
#
# IMPORTED Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines :prop_tgt:`IMPORTED` target ``VSG::VSG``, if
# VSG has been found.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables::
#
#   VSG_FOUND           - True if VSG was found
#   VSG_INCLUDE_DIRS    - include directories for VSG and third-party VSG-Hpp
#   VSG_LIBRARIES       - link against this library to use VSG
#
# The module will also define the following cache variables::
#
#   VSG_INCLUDE_DIR     - the VSG include directory
#   VSG_LIBRARY         - the path to the VSG library
#

# VSG header file
find_path(VSG_INCLUDE_DIR
  NAMES vsg/all.h
  PATHS
    "$ENV{VSG_DIR}/include"
  NO_DEFAULT_PATH
  )

find_library(VSG_LIBRARY
    NAMES vsg
    PATHS
    "$ENV{VSG_DIR}/lib")

set(VSG_LIBRARIES ${VSG_LIBRARY})
set(VSG_INCLUDE_DIRS ${VSG_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VSG
  DEFAULT_MSG
  VSG_LIBRARY VSG_INCLUDE_DIR)

# mark_as_advanced(VSG_INCLUDE_DIR VSG_LIBRARY)

if(VSG_FOUND AND NOT TARGET VSG::VSG)
  add_library(VSG::VSG UNKNOWN IMPORTED)
  set_target_properties(VSG::VSG PROPERTIES
    IMPORTED_LOCATION "${VSG_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${VSG_INCLUDE_DIRS}")
endif()
