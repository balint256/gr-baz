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

#ifndef INCLUDED_BAZ_TIME_KEEPER_H
#define INCLUDED_BAZ_TIME_KEEPER_H

#include <gnuradio/sync_block.h>
//#include <gnuradio/msg_queue.h>
#include <gnuradio/thread/thread.h>

class BAZ_API baz_time_keeper;

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
typedef boost::shared_ptr<baz_time_keeper> baz_time_keeper_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_time_keeper.
 *
 * To avoid accidental use of raw pointers, baz_time_keeper's
 * constructor is private.  howto_make_square2_ff is the public
 * interface for creating new instances.
 */
BAZ_API baz_time_keeper_sptr baz_make_time_keeper (int item_size, int sample_rate);

/*!
 * \brief square2 a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr::sync_block.
 */
class BAZ_API baz_time_keeper : public gr::sync_block
{
private:
	// The friend declaration allows baz_time_keeper to
	// access the private constructor.

	friend BAZ_API baz_time_keeper_sptr baz_make_time_keeper (int item_size, int sample_rate);

	baz_time_keeper (int item_size, int sample_rate);  	// private constructor

	int d_item_size;
	uint64_t d_last_time_seconds, d_first_time_seconds;
	double d_last_time_fractional_seconds, d_first_time_fractional_seconds;
	uint64_t d_time_offset;
	int d_sample_rate;
	bool d_seen_time;
	int d_update_count;
	bool d_ignore_next;
	gr::thread::mutex d_mutex;
	pmt::pmt_t d_status_port_id;

public:
	~baz_time_keeper ();	// public destructor

	double time(bool relative = false);
	void ignore_next(bool ignore = true);
	int update_count(void);

	int work (int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_TIME_KEEPER_H */
