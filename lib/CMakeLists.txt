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
# Install public header files
########################################################################
SET(baz_headers
	baz_print_char.h
	baz_unpacked_to_packed_bb.h
	baz_pow_cc.h
	baz_delay.h
	baz_puncture_bb.h
	baz_depuncture_ff.h
	baz_swap_ff.h
	baz_agc_cc.h
	baz_test_counter_cc.h
	baz_native_callback.h
	baz_native_mux.h
	baz_block_status.h
	baz_non_blocker.h
	baz_time_keeper.h
	baz_burster.h
	baz_radar_detector.h
	baz_fastrak_decoder.h
	baz_overlap.h
	baz_manchester_decode_bb.h
	baz_sweep.h
	baz_merge.h
	baz_tcp_sink.h
	baz_tcp_source.h
	baz_auto_ber_bf.h
	baz_peak_detector.h
	baz_burst_tagger.h
	baz_burst_buffer.h
)

if (LIBUSB_FOUND)
	list(APPEND baz_headers baz_rtl_source_c.h)
endif ()

if (UHD_FOUND)
	list(APPEND baz_headers baz_gate.h baz_usrp_acquire.h baz_hopper.h)
endif ()

install(FILES
    ${baz_headers}
    DESTINATION include/baz
)

########################################################################
# Setup library
########################################################################
include(GrPlatform) #define LIB_SUFFIX

SET(baz_sources
	baz_print_char.cc
	baz_unpacked_to_packed_bb.cc
	baz_pow_cc.cc
	baz_delay.cc
	baz_puncture_bb.cc
	baz_depuncture_ff.cc
	baz_swap_ff.cc
	baz_agc_cc.cc
	baz_test_counter_cc.cc
	baz_udp_source.cc
	baz_udp_sink.cc
	baz_native_callback.cc
	baz_native_mux.cc
	baz_block_status.cc
	baz_non_blocker.cc

	baz_acars_decoder.cc
	baz_tag_to_msg.cc
	baz_time_keeper.cc
	baz_burster.cc
	baz_radar_detector.cc
	baz_fastrak_decoder.cc
	baz_overlap.cc
	baz_manchester_decode_bb.cc
	baz_sweep.cc
	baz_merge.cc
	baz_tcp_sink.cc
	baz_tcp_source.cc
	baz_auto_ber_bf.cc
	baz_peak_detector.cc
	baz_burst_tagger_impl.cc
	baz_burst_buffer.cc
)

if (LIBUSB_FOUND)
	LIST(APPEND baz_sources
		baz_rtl_source_c.cc
		rtl2832.cc
		rtl2832-tuner_e4000.cc
		rtl2832-tuner_fc0013.cc
		rtl2832-tuner_fc0012.cc
		rtl2832-tuner_fc2580.cc
		rtl2832-tuner_r820t.cc
		rtl2832-tuner_e4k.cc
	)
else ()
	SET(LIBUSB_LIBRARIES "")
endif ()

if (UHD_FOUND)
	LIST(APPEND baz_sources baz_gate.cc baz_usrp_acquire.cc baz_hopper.cc)
else ()
	SET(UHD_LIBRARIES "")
endif ()

if (ARMADILLO_FOUND)
	LIST(APPEND baz_sources baz_music_doa.cc)
else ()
	set(ARMADILLO_LIBRARIES)
endif ()

add_library(gnuradio-baz SHARED
	${baz_sources}
)

list(APPEND baz_libs
	${Boost_LIBRARIES}
	${GNURADIO_RUNTIME_LIBRARIES}
	${GNURADIO_PMT_LIBRARIES}
	${GNURADIO_BLOCKS_LIBRARIES}
	${GNURADIO_DIGITAL_LIBRARIES}
	${LIBUSB_LIBRARIES}
	${UHD_LIBRARIES}
	${ARMADILLO_LIBRARIES}
)

if(MSVC)
	CHECK_INCLUDE_FILE_CXX(windows.h HAVE_WINDOWS_H)
	IF(HAVE_WINDOWS_H)
		ADD_DEFINITIONS(-DHAVE_WINDOWS_H -DUSING_WINSOCK)
		MESSAGE(STATUS "Adding windows libs to gr baz libs...")
		LIST(APPEND baz_libs WS2_32.lib WSock32.lib)
	ENDIF(HAVE_WINDOWS_H)
endif()

target_link_libraries(gnuradio-baz ${baz_libs})
set_target_properties(gnuradio-baz PROPERTIES DEFINE_SYMBOL "gnuradio_baz_EXPORTS;RTL2832_EXPORTS")

########################################################################
# Install built library files
########################################################################
if(ENABLE_GR_BAZ)
    GR_LIBRARY_FOO(gnuradio-baz RUNTIME_COMPONENT "baz_runtime" DEVEL_COMPONENT "baz_devel")
else()
    install(TARGETS gnuradio-baz
        LIBRARY DESTINATION lib${LIB_SUFFIX} # .so/.dylib file
        ARCHIVE DESTINATION lib${LIB_SUFFIX} # .lib file
        RUNTIME DESTINATION bin              # .dll file
    )
endif()

########################################################################
# Build and register unit test
########################################################################
#find_package(Boost COMPONENTS unit_test_framework)

#include(GrTest)
#set(GR_TEST_TARGET_DEPS gnuradio-howto)
#turn each test cpp file into an executable with an int main() function
#add_definitions(-DBOOST_TEST_DYN_LINK -DBOOST_TEST_MAIN)

#add_executable(qa_howto_square_ff qa_howto_square_ff.cc)
#target_link_libraries(qa_howto_square_ff gnuradio-howto ${Boost_LIBRARIES})
#GR_ADD_TEST(qa_howto_square_ff qa_howto_square_ff)

#add_executable(qa_howto_square2_ff qa_howto_square2_ff.cc)
#target_link_libraries(qa_howto_square2_ff gnuradio-howto ${Boost_LIBRARIES})
#GR_ADD_TEST(qa_howto_square2_ff qa_howto_square2_ff)

