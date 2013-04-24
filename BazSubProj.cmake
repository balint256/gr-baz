# Copyright 2013 Free Software Foundation, Inc.
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
# Setup baz as a subproject
########################################################################
include(GrComponent)
GR_REGISTER_COMPONENT("gr-baz" ENABLE_GR_BAZ)

if(ENABLE_GR_BAZ)

include(GrPackage)
CPACK_SET(CPACK_COMPONENT_GROUP_BAZ_DESCRIPTION "Baz Blocks")

CPACK_COMPONENT("baz_runtime"
    GROUP        "Baz"
    DISPLAY_NAME "Runtime"
    DESCRIPTION  "Dynamic link libraries"
    DEPENDS      "core_runtime;gruel_runtime"
)

CPACK_COMPONENT("baz_devel"
    GROUP        "Baz"
    DISPLAY_NAME "Development"
    DESCRIPTION  "C++ headers, package config, import libraries"
    DEPENDS      "core_devel;gruel_devel"
)

CPACK_COMPONENT("baz_python"
    GROUP        "Baz"
    DISPLAY_NAME "Python"
    DESCRIPTION  "Python modules for runtime; GRC xml files"
    DEPENDS      "core_python;gruel_python;baz_runtime"
)

CPACK_COMPONENT("baz_swig"
    GROUP        "Baz"
    DISPLAY_NAME "SWIG"
    DESCRIPTION  "SWIG development .i files"
    DEPENDS      "core_swig;gruel_swig;baz_python;baz_devel"
)

add_subdirectory(${GR_BAZ_SRC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/gr-baz)
endif(ENABLE_GR_BAZ)
