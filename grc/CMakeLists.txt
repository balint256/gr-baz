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

SET(baz_blocks
	baz_any.xml
	baz_any_source.xml
	baz_any_sink.xml
	baz_pow_cc.xml
	baz_delay.xml
	baz_puncture_bb.xml
	baz_depuncture_ff.xml
	baz_swap_ff.xml
	facsink.xml
	baz_borip_source.xml
	eye.xml
	baz_agc_xx.xml
	baz_test_counter_cc.xml
	gr_mpsk_receiver_debug_cc.xml
	baz_auto_fec.xml
	op25.xml
	baz_message_callback.xml
	baz_borip_sink.xml
	baz_udp_source.xml
	baz_udp_sink.xml
	baz_block_status.xml
	baz_any_single.xml
	baz_non_blocker.xml
)

if (LIBUSB_FOUND)
	LIST(APPEND baz_blocks baz_rtl_source_c.xml)
endif ()

if (UHD_FOUND)
	LIST(APPEND baz_blocks baz_gate.xml)
endif ()

install(FILES
    ${baz_blocks}
    DESTINATION share/gnuradio/grc/blocks
)

