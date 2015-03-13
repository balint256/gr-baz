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

#ifndef INCLUDED_BAZ_GATE_H
#define INCLUDED_BAZ_GATE_H

#include <gnuradio/sync_block.h>
//#include <gnuradio/msg_queue.h>
#include <uhd/types/time_spec.hpp>
#include <gnuradio/thread/thread.h>

class BAZ_API baz_gate;

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
typedef boost::shared_ptr<baz_gate> baz_gate_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_gate.
 *
 * To avoid accidental use of raw pointers, baz_gate's
 * constructor is private.  howto_make_square2_ff is the public
 * interface for creating new instances.
 */
BAZ_API baz_gate_sptr baz_make_gate (int item_size, bool block = true, float threshold = 1.0, int trigger_length = 0, bool tag = false, double delay = 0.0, int sample_rate = 0, bool no_delay = false, bool verbose = true, bool retriggerable = false);

/*!
 * \brief square2 a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr::sync_block.
 */
class BAZ_API baz_gate : public gr::block
{
private:
  // The friend declaration allows howto_make_square2_ff to
  // access the private constructor.

  friend BAZ_API baz_gate_sptr baz_make_gate (int item_size, bool block, float threshold, int trigger_length, bool tag, double delay, int sample_rate, bool no_delay, bool verbose, bool retriggerable);

  baz_gate (int item_size, bool block, float threshold, int trigger_length, bool tag, double delay, int sample_rate, bool no_delay, bool verbose, bool retriggerable);  	// private constructor

  int d_item_size;
  bool d_block;
  float d_threshold;
  int d_trigger_length;
  int d_trigger_count;
  bool d_tag;
  double d_delay;
  uhd::time_spec_t d_last_time;
  uint64_t d_time_offset;
  int d_sample_rate;
  bool d_in_burst;
  int d_output_index;
  bool d_no_delay;
  gr::thread::mutex d_mutex;
  bool d_verbose;
  bool d_retriggerable;
  int d_flush_length;
  int d_flush_count;

public:
  ~baz_gate ();	// public destructor

  void set_blocking(bool enable);
  void set_threshold(float threshold);
  void set_trigger_length(int trigger_length);
  void set_tagging(bool enable);
  void set_delay(double delay);
  void set_sample_rate(int sample_rate);
  void set_no_delay(bool no_delay);

  void forecast(int noutput_items, gr_vector_int &ninput_items_required);

  int general_work (int noutput_items, gr_vector_int &ninput_items,
	    gr_vector_const_void_star &input_items,
	    gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_NATIVE_MUX_H */
