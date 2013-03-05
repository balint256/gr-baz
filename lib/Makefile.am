#
# Copyright 2004,2005,2006,2008,2009,2010 Free Software Foundation, Inc.
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
# 

include $(top_srcdir)/Makefile.common

# list of programs run by "make check" and "make distcheck"
TESTS = test_all

# ----------------------------------------------------------------
# baz C++ library: libgnuradio-baz.so
# ----------------------------------------------------------------

# C/C++ headers get installed in ${prefix}/include/$(modname)
modinclude_HEADERS = \
	baz_print_char.h \
	baz_unpacked_to_packed_bb.h \
	baz_pow_cc.h \
	baz_delay.h \
	baz_puncture_bb.h \
	baz_depuncture_ff.h \
	baz_swap_ff.h \
	baz_agc_cc.h \
	baz_test_counter_cc.h \
	baz_rtl_source_c.h
#	howto_square_ff.h \
#	howto_square2_ff.h

lib_LTLIBRARIES = libgnuradio-baz.la

libgnuradio_baz_la_SOURCES = \
	baz_print_char.cc \
	baz_unpacked_to_packed_bb.cc \
	baz_pow_cc.cc \
	baz_delay.cc \
	baz_puncture_bb.cc \
	baz_depuncture_ff.cc \
	baz_swap_ff.cc \
	baz_agc_cc.cc \
	baz_test_counter_cc.cc \
	baz_rtl_source_c.cc \
	rtl2832.cc \
	rtl2832-tuner_e4000.cc \
	rtl2832-tuner_fc0013.cc \
	rtl2832-tuner_fc0012.cc \
	rtl2832-tuner_fc2580.cc \
	rtl2832-tuner_r820t.cc \
	rtl2832-tuner_e4k.cc \
	baz_udp_source.cc \
	baz_udp_sink.cc
#	howto_square_ff.cc \
#	howto_square2_ff.cc

libgnuradio_baz_la_LIBADD = \
	$(GNURADIO_CORE_LA) -lusb-1.0

libgnuradio_baz_la_LDFLAGS = \
	$(NO_UNDEFINED)

# ----------------------------------------------------------------
# baz C++ QA library: libgnuradio-baz-qa.so (not installed)
# ----------------------------------------------------------------

noinst_LTLIBRARIES = libgnuradio-baz-qa.la

libgnuradio_baz_qa_la_SOURCES = \
	qa_baz.cc
#	qa_howto_square_ff.cc \
#	qa_howto_square2_ff.cc

libgnuradio_baz_qa_la_LDFLAGS = $(NO_UNDEFINED) -version-info 0:0:0

libgnuradio_baz_qa_la_LIBADD = \
	libgnuradio-baz.la \
	$(CPPUNIT_LIBS)

# ----------------------------------------------------------------
# headers that don't get installed
# ----------------------------------------------------------------
noinst_HEADERS = \
	qa_baz.h
#	qa_howto_square_ff.h \
#	qa_howto_square2_ff.h

# ----------------------------------------------------------------
# test program
# ----------------------------------------------------------------
noinst_PROGRAMS = \
	test_all

test_all_SOURCES = test_all.cc
test_all_LDADD   = libgnuradio-baz-qa.la
