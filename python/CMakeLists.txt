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
# Include python install macros
########################################################################
include(GrPython)
if(NOT PYTHONINTERP_FOUND)
    return()
endif()

########################################################################
# Install python sources
########################################################################
SET(baz_python_sources
	__init__.py
	baudline.py
)

if (IN_GR_BAZ_3_8)

# No other Py until made Py3.7 compat

else ()

LIST(APPEND baz_python_sources
	borip.py
	facsink.py
	eye.py
	auto_fec.py
	baz_op25.py
	op25_traffic_pane.py
	message_callback.py
	borip_server.py
	std_flusher.py

	plot_sink.py
	plot_window.py
	multi_channel_decoder.py
	acars_printer.py
	message_relay.py

	music_doa_helper.py
	parallel_scanner_fsm.py
	time_panel.py
	time_panel_gen.py

	missile_launcher.py
	doa_compass_plotter.py
	doa_compass_control.py
	message_server.py
	radar_server.py
	colours.py
	static_text.py

	introspective_xmlrpc_server.py
	usrp.py

	waterfall_sink.py
	waterfall_window.py

	correlator.py
	delayed_start.py
	horizons_block.py
	rx_time.py
	fec_sync.py
	usrp_agc.py
	freq_hopper.py
	wx_flipper.py
	rate_synchroniser.py
)

endif ()

GR_PYTHON_INSTALL(
    FILES
    ${baz_python_sources}
    DESTINATION ${GR_PYTHON_DIR}/baz
)

########################################################################
# Handle the unit tests
########################################################################
#include(GrTest)

#set(GR_TEST_TARGET_DEPS gnuradio-baz)
#set(GR_TEST_PYTHON_DIRS ${CMAKE_BINARY_DIR}/swig)
#GR_ADD_TEST(qa_baz ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/qa_baz.py)
