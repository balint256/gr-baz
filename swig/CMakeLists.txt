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

########################################################################
# Include swig generation macros
########################################################################
find_package(SWIG)
find_package(PythonLibs)
if(NOT SWIG_FOUND OR NOT PYTHONLIBS_FOUND)
    return()
endif()
include(GrSwig)
include(GrPython)

########################################################################
# Setup swig generation
########################################################################

list(APPEND GR_SWIG_INCLUDE_DIRS /Users/mrene/Documents/Sources/gnuradio/gnuradio-runtime/swig)
list(APPEND GR_SWIG_INCLUDE_DIRS /Users/mrene/Documents/Sources/gnuradio/build/gnuradio-runtime/swig/)
if(ENABLE_GR_BAZ)
    list(APPEND GR_SWIG_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/gruel/src/swig)
    list(APPEND GR_SWIG_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/gruel/src/swig)
    list(APPEND GR_SWIG_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/gnuradio-core/src/lib/swig)
    list(APPEND GR_SWIG_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/gnuradio-core/src/lib/swig)
    if(UHD_FOUND)
        list(APPEND GR_SWIG_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/gr-uhd/swig)
    endif()
endif()

foreach(incdir ${GNURADIO_RUNTIME_INCLUDE_DIRS})
    list(APPEND GR_SWIG_INCLUDE_DIRS ${incdir}/swig)
endforeach(incdir)

foreach(incdir ${GRUEL_INCLUDE_DIRS})
    list(APPEND GR_SWIG_INCLUDE_DIRS ${incdir}/gruel/swig)
endforeach(incdir)

set(GR_SWIG_LIBRARIES gnuradio-baz)
set(GR_SWIG_DOC_FILE ${CMAKE_CURRENT_BINARY_DIR}/baz_swig_doc.i)
#set(GR_SWIG_DOC_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../include)
set(GR_SWIG_DOC_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../lib)

GR_SWIG_MAKE(baz_swig baz_swig.i)

########################################################################
# Install the build swig module
########################################################################
GR_SWIG_INSTALL(TARGETS baz_swig DESTINATION ${GR_PYTHON_DIR}/baz)
#GR_SWIG_INSTALL(TARGETS baz DESTINATION ${GR_PYTHON_DIR}/baz)

########################################################################
# Install swig .i files for development
########################################################################
install(
    FILES
	baz_swig.i
    ${CMAKE_CURRENT_BINARY_DIR}/baz_swig_doc.i
    DESTINATION ${GR_INCLUDE_DIR}/baz/swig
)

