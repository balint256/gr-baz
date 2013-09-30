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

#ifndef INCLUDED_BAZ_NATIVE_MUX_H
#define INCLUDED_BAZ_NATIVE_MUX_H

#include <gnuradio/sync_block.h>
#include <baz_native_callback.h>

class BAZ_API baz_native_mux;

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
typedef boost::shared_ptr<baz_native_mux> baz_native_mux_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_native_mux.
 *
 * To avoid accidental use of raw pointers, baz_native_mux's
 * constructor is private.  howto_make_square2_ff is the public
 * interface for creating new instances.
 */
BAZ_API baz_native_mux_sptr baz_make_native_mux (int item_size, int input_count, int trigger_count = -1);

/*!
 * \brief square2 a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr::sync_block.
 */
class BAZ_API baz_native_mux : public /*gr::sync_block*/gr::block, public baz_native_callback_target
{
private:
  // The friend declaration allows howto_make_square2_ff to
  // access the private constructor.

  friend BAZ_API baz_native_mux_sptr baz_make_native_mux (int item_size, int input_count, int trigger_count);

  baz_native_mux (int item_size, int input_count, int trigger_count);  	// private constructor

  int d_item_size;
  int d_input_count;
  int d_selected_input;
  int d_trigger_count;
  int d_trigger_countdown;
  std::vector<float> d_values;
  int d_value_index;
  int d_last_noutput_items;

  unsigned long d_samples_processed;
  std::vector<unsigned long> d_switch_time;

 public:
  ~baz_native_mux ();	// public destructor

  int general_work (int noutput_items, gr_vector_int &ninput_items,
	    gr_vector_const_void_star &input_items,
	    gr_vector_void_star &output_items);

public:
  void callback(float f, unsigned long samples_processed);
};

#endif /* INCLUDED_BAZ_NATIVE_MUX_H */
