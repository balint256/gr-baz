#!/usr/bin/env python

import sys
import os

if __name__ == '__main__':
    if len(sys.argv) == 1:
        print 'usage: gr_baz_attach.py [path to gnuradio src tree]'
        exit()
    top_gr_cmakelists = os.path.join(sys.argv[1], 'CMakeLists.txt')
    gr_baz_src_dir = os.path.dirname(os.path.abspath(__file__)).replace("\\","\\\\")
    content = open(top_gr_cmakelists).read()
    if 'BazSubProj.cmake' not in content:
        content = content.replace('add_subdirectory(grc)',
"""add_subdirectory(grc)
file(TO_CMAKE_PATH %s GR_BAZ_SRC_DIR)
include(${GR_BAZ_SRC_DIR}/BazSubProj.cmake)"""%gr_baz_src_dir)
    open(top_gr_cmakelists, 'w').write(content)
