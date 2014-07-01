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

#ifndef INCLUDED_BAZ_MERGE_H
#define INCLUDED_BAZ_MERGE_H

#include <gnuradio/sync_block.h>

#include <vector>

class BAZ_API baz_merge;

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
typedef boost::shared_ptr<baz_merge> baz_merge_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_merge.
 *
 * To avoid accidental use of raw pointers, baz_merge's
 * constructor is private.  howto_make_square2_ff is the public
 * interface for creating new instances.
 */
BAZ_API baz_merge_sptr baz_make_merge(int item_size, float samp_rate, int additional_streams = 1, bool drop_residual = true, const char* length_tag = "length", const char* ignore_tag = "ignore");

/*!
 * \brief square2 a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr::sync_block.
 */
class BAZ_API baz_merge : public gr::block
{
private:
	// The friend declaration allows howto_make_square2_ff to
	// access the private constructor.

	friend BAZ_API baz_merge_sptr baz_make_merge (int item_size, float samp_rate, int additional_streams, bool drop_residual, const char* length_tag, const char* ignore_tag);

	baz_merge (int item_size, float samp_rate, int additional_streams, bool drop_residual, const char* length_tag, const char* ignore_tag);  	// private constructor

	float d_samp_rate;
	bool d_drop_residual;
	uint64_t d_start_time_whole;
	double d_start_time_frac;
	int d_selected_input;
	int d_items_to_copy;
	//int d_items_to_ignore;
	bool d_ignore_current;
	pmt::pmt_t d_length_name, d_ignore_name;
	std::vector<pmt::pmt_t> msg_output_ids;
	uint64_t d_total_burst_count;

public:
	~baz_merge ();	// public destructor

	void set_start_time(double time);
	void set_start_time(uint64_t whole, double frac);
  
	//inline float exponent() const
	//{ return d_exponent; }

	void forecast(int noutput_items, gr_vector_int &ninput_items_required);
	int general_work (int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_MERGE_H */
