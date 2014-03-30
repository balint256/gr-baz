/* -*- c++ -*- */
/*
 * Copyright 2004,2013 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * gr-baz by Balint Seeber (http://spench.net/contact)
 * Information, documentation & samples: http://wiki.spench.net/wiki/gr-baz
 */

#ifndef INCLUDED_BAZ_RADAR_DETECTOR_H
#define INCLUDED_BAZ_RADAR_DETECTOR_H

#include <gnuradio/block.h>
#include <gnuradio/msg_queue.h>

class BAZ_API baz_radar_detector;

/*
 * We use boost::shared_ptr's instead of raw pointers for all access
 * to gr_blocks (and many other data structures).  The shared_ptr gets
 * us transparent reference counting, which greatly simplifies storage
 * management issues.  This is especially helpful in our hybrid
 * C++ / Python system.
 *
 * See http://www.boost.org/libs/smart_ptr/smart_ptr.htm
 *
 * As a convention, the _sptr suffix indicates a boost::shared_ptr
 */
typedef boost::shared_ptr<baz_radar_detector> baz_radar_detector_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_radar_detector.
 *
 * To avoid accidental use of raw pointers, baz_radar_detector's
 * constructor is private.  baz_make_block_status is the public
 * interface for creating new instances.
 */
BAZ_API baz_radar_detector_sptr baz_make_radar_detector (int sample_rate, gr::msg_queue::sptr msgq);

/*!
 * \brief radar_detector a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr_sync_block.
 */
class BAZ_API baz_radar_detector : public gr::block
{
private:
	// The friend declaration allows baz_make_block_status to
	// access the private constructor.
	friend BAZ_API baz_radar_detector_sptr baz_make_radar_detector (int sample_rate, gr::msg_queue::sptr msgq);

	baz_radar_detector (int sample_rate, gr::msg_queue::sptr msgq);  	// private constructor

	int d_sample_rate;
	gr::msg_queue::sptr d_msgq;
	float d_base_level;
	float d_threshold;
	bool d_in_burst;
	float d_first;
	uint64_t d_burst_start, d_burst_flat_start;
	double d_sum, d_max;
	int d_skip;
	float d_pulse_plateau;
	float d_last;
	double d_flat_sum;
	bool d_in_plateau;
	int d_flat_sum_count;
public:
	struct ath5k_radar_error {
		uint32_t tsf;
		uint8_t rssi;
		uint8_t width;   // 0 for WiFi frame
		uint8_t type;    // 0: WiFi frame (no error), 1: RADAR PHY error
		uint8_t subtype; // For type 0: 0; type 1: rs_rate
	};
public:
	~baz_radar_detector ();	// public destructor

	void set_base_level(float level);
	void set_threshold(float threshold);
	void set_pulse_plateau(float level);
	bool set_param(const std::string& param, float value);
	void skip(int skip);

	int general_work (int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_RADAR_DETECTOR_H */
