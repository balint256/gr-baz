/* -*- c++ -*- */
/*
 * Copyright 2007,2013 Free Software Foundation, Inc.
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

#ifndef INCLUDED_BAZ_BURST_BUFFER_H
#define INCLUDED_BAZ_BURST_BUFFER_H

#include <gnuradio/block.h>
#include <boost/thread.hpp>

class BAZ_API baz_burst_buffer;
typedef boost::shared_ptr<baz_burst_buffer> baz_burst_buffer_sptr;

BAZ_API baz_burst_buffer_sptr baz_make_burst_buffer (size_t itemsize, int flush_length = 0, bool verbose = false);

/*!
 * \brief buffer bursts
 * \ingroup misc_blk
 */
class BAZ_API baz_burst_buffer : public gr::block
{
	friend BAZ_API baz_burst_buffer_sptr baz_make_burst_buffer (size_t itemsize, int flush_length, bool verbose);

	baz_burst_buffer (size_t itemsize, int flush_length = 0, bool verbose = false);

	//boost::mutex d_mutex;
	size_t d_itemsize;
	size_t d_buffer_size;
	void* d_buffer;
	size_t d_sample_count;
	bool d_in_burst;
	bool d_add_sob;
	int d_flush_length;
	int d_flush_count;
	bool d_verbose;

public:
	~baz_burst_buffer();
	
	//int delay () const { return d_delay; }
	//void set_delay (int delay);
	
	void reallocate_buffer(void);

	void forecast(int noutput_items, gr_vector_int &ninput_items_required);
	int general_work (int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif
