/* -*- c++ -*- */
/*
 * Copyright 2004 Free Software Foundation, Inc.
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

#ifndef INCLUDED_BAZ_PEAK_DETECTOR_H
#define INCLUDED_BAZ_PEAK_DETECTOR_H

#include <gnuradio/sync_block.h>

class BAZ_API baz_peak_detector;

/*
 * We use boost::shared_ptr's instead of raw pointers for all access
 * to gr::blocks (and many other data structures).  The shared_ptr gets
 * us transparent reference counting, which greatly simplifies storage
 * management issues.  This is especially helpful in our hybrid
 * C++ / Python system.
 *
 * See http://www.boost.org/libs/smart_ptr/smart_ptr.htm
 *
 * As a convention, the _sptr suffix indicates a boost::shared_ptr
 */
typedef boost::shared_ptr<baz_peak_detector> baz_peak_detector_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_peak_detector.
 *
 * To avoid accidental use of raw pointers, baz_peak_detector's
 * constructor is private.  howto_make_square2_ff is the public
 * interface for creating new instances.
 */
BAZ_API baz_peak_detector_sptr baz_make_peak_detector (float min_diff = 0.0, int min_len = 1, int lockout = 0, float drop = 0.0, float alpha = 1.0, int look_ahead = 0);

/*!
 * \brief square2 a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr::sync_block.
 */
class BAZ_API baz_peak_detector : public gr::sync_block
{
private:
	// The friend declaration allows howto_make_square2_ff to
	// access the private constructor.

	friend BAZ_API baz_peak_detector_sptr baz_make_peak_detector (float min_diff, int min_len, int lockout, float drop, float alpha, int look_ahead);

	baz_peak_detector(float min_diff, int min_len, int lockout, float drop, float alpha, int look_ahead);  	// private constructor

	float d_min_diff;
	int d_min_len;
	int d_lockout;
	float d_drop;
	float d_alpha;
	int d_look_ahead;
	
	bool d_rising;
	int d_rise_count;
	int d_lockout_count;
	float d_first;
	float d_ave;
	float d_peak;
	int d_peak_idx;
	int d_look_ahead_count;

	public:
	~baz_peak_detector ();	// public destructor

	//void set_exponent(float exponent);

	//inline float exponent() const
	//{ return d_exponent; }

	int work (int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_PEAK_DETECTOR_H */
