# Copyright 2011 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.

# GR 3.7 & 3.8 compat

########################################################################
# Check if there is C++ code at all
########################################################################
if(NOT baz_sources)
    MESSAGE(STATUS "No C++ sources... skipping swig/")
    return()
endif(NOT baz_sources)

########################################################################
# Include swig generation macros
########################################################################
find_package(SWIG)

if (IN_GR_BAZ_3_8)

find_package(PythonLibs)

else ()

find_package(PythonLibs 2)
#set(Python_ADDITIONAL_VERSIONS "2.7")
#find_package(PythonLibs 2.7 REQUIRED)

endif ()

if(NOT SWIG_FOUND OR NOT PYTHONLIBS_FOUND)
    return()
endif()
include(GrSwig)
include(GrPython)

########################################################################
# Setup swig generation
########################################################################

#if(ENABLE_GR_BAZ)

if (IN_GR_BAZ_3_8)

set(GR_SWIG_INCLUDE_DIRS $<TARGET_PROPERTY:gnuradio::runtime_swig,INTERFACE_INCLUDE_DIRECTORIES>)
set(GR_SWIG_TARGET_DEPS gnuradio::runtime_swig)

else ()

foreach(incdir ${GNURADIO_RUNTIME_INCLUDE_DIRS})
    list(APPEND GR_SWIG_INCLUDE_DIRS ${incdir}/gnuradio/swig)
endforeach(incdir)

endif()

set(GR_SWIG_LIBRARIES gnuradio-baz)

set(GR_SWIG_DOC_FILE ${CMAKE_CURRENT_BINARY_DIR}/baz_swig_doc.i)
#set(GR_SWIG_DOC_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../include)
set(GR_SWIG_DOC_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../lib)

GR_SWIG_MAKE(baz_swig baz_swig.i)

########################################################################
# Install the build swig module
########################################################################
GR_SWIG_INSTALL(TARGETS baz_swig DESTINATION ${GR_PYTHON_DIR}/baz)

########################################################################
# Install swig .i files for development
########################################################################
install(
    FILES
    baz_swig.i
    ${CMAKE_CURRENT_BINARY_DIR}/baz_swig_doc.i
    DESTINATION ${GR_INCLUDE_DIR}/baz/swig
)

#endif()
