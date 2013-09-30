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

#ifndef INCLUDED_BAZ_BURSTER_H
#define INCLUDED_BAZ_BURSTER_H

//#include <sys/time.h>
#include <boost/thread/thread.hpp>

#include <string>
#include <vector>
#include <map>

#include <gnuradio/sync_block.h>
#include <gnuradio/msg_queue.h>
#include <pmt/pmt.h>

#include "baz_burster_config.h"

class BAZ_API baz_burster;

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
typedef boost::shared_ptr<baz_burster> baz_burster_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_burster.
 *
 * To avoid accidental use of raw pointers, baz_burster's
 * constructor is private.  baz_make_burster is the public
 * interface for creating new instances.
 */
BAZ_API baz_burster_sptr baz_make_burster (const baz_burster_config& config);

/*!
 * \brief burster a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr::sync_block.
 */
class BAZ_API baz_burster : public gr::block
{
private:
	friend BAZ_API baz_burster_sptr baz_make_burster (const baz_burster_config& config);

	baz_burster (const baz_burster_config& config);  	// private constructor

	baz_burster_config d_config;
	
	typedef struct burst_time_t
	{
		uint64_t seconds;
		double fractional_seconds;
		uint64_t sample_offset;
		int sample_rate;
	} burst_time;
	
	inline static burst_time burst_time_absorb_whole_samples(const burst_time& t)
	{
		burst_time _t(t);
		uint64_t s = t.sample_offset / t.sample_rate;
		_t.seconds += s;
		_t.sample_offset -= (s * t.sample_rate);
		return _t;
	}
	
	inline static burst_time burst_time_absorb_samples(const burst_time& t)
	{
		burst_time _t(t);
		double d = (double)t.sample_offset / (double)t.sample_rate;
		_t.seconds += (int)d;
		_t.fractional_seconds += (d - (int)d);
		_t.sample_offset = 0;
		return _t;
	}
	
	inline static burst_time burst_time_difference(const burst_time& t1, const burst_time& t2, bool relative_to_second = true)
	{
		burst_time t;
		
		if ((t1.sample_rate == t2.sample_rate)/* && (t1.seconds == t2.seconds) && (t1.fractional_seconds == t2.fractional_seconds)*/)
		{
			t.sample_rate = t1.sample_rate;
			t.seconds = 0;
			t.fractional_seconds = 0;
			t.sample_offset =
				(t1.sample_rate * (t1.seconds - t2.seconds)) +
				(uint64_t)((double)t1.sample_rate * (t1.fractional_seconds - t2.fractional_seconds)) +
				(t1.sample_offset - t2.sample_offset);
			return t;
		}
		
		if ((t1.sample_rate != t2.sample_rate) && ((t1.sample_offset > 0) || (t2.sample_offset > 0)))
		{
			return burst_time_difference(burst_time_absorb_samples(t1), burst_time_absorb_samples(t2));
		}
		
		t.seconds = 0;
		t.fractional_seconds = 0;
		
		if (relative_to_second)
		{
			t.sample_rate = t2.sample_rate;
		}
		else
		{
			t.sample_rate = t1.sample_rate;
		}
		
		t.sample_offset = (t.sample_rate * (t1.seconds - t2.seconds)) + (uint64_t)((double)t.sample_rate * (t1.fractional_seconds - t2.fractional_seconds));
		
		return t;
	}
	
	bool send_pending_msg(void);
	void set_burst_length(int length);
//////////////////// ZERO
	union {
		char d_dummy_zero_first;
		//uint64_t d_last_time_seconds;
		burst_time d_stream_time;
	};
	//double d_last_time_fractional_seconds;
	burst_time d_last_burst_time;
	int d_burst_message_sample_index;
	//bool d_msg_ready;
	bool d_in_burst;
	int d_burst_count;	// Incremented when new one is started (not when it is transmitted)
	bool d_last_burst_system_time_valid;
	int d_time_tags;
	uint64_t d_current_burst_length;
	char* d_message_buffer;
	int d_message_buffer_length;
	int d_system_time_ticks_per_second;
	// Next must come last
	char d_dummy_zero_last;
////////////////////
	//uint64_t d_last_burst_sample_time;
	//double d_last_burst_time;
	boost::system_time d_system_time;
	boost::system_time d_last_burst_system_time;
	//uint64_t d_samples_since_last_time_tag;
	//gr::message::sptr d_current_msg;
	gr::message::sptr d_pending_msg;
	std::vector<gr::tag_t> d_incoming_time_tags;
public:
	~baz_burster ();	// public destructor

	//

	void forecast(int noutput_items, gr_vector_int &ninput_items_required);
	int general_work (int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_BURSTER_H */
