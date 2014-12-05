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

#ifndef INCLUDED_BAZ_DELAY_H
#define INCLUDED_BAZ_DELAY_H

#include <gnuradio/sync_block.h>
#include <boost/thread.hpp>

class BAZ_API baz_delay;
typedef boost::shared_ptr<baz_delay> baz_delay_sptr;

BAZ_API baz_delay_sptr baz_make_delay (size_t itemsize, int delay);

/*!
 * \brief delay the input by a certain number of samples
 * \ingroup misc_blk
 */
class BAZ_API baz_delay : public gr::block
{
	friend BAZ_API baz_delay_sptr baz_make_delay (size_t itemsize, int delay);

	baz_delay (size_t itemsize, int delay);

	boost::mutex d_mutex;
	size_t d_itemsize;
	int d_delay;

public:
	int delay () const { return d_delay; }
	void set_delay (int delay);

	void forecast(int noutput_items, gr_vector_int &ninput_items_required);
	int general_work (int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif
