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

#ifndef INCLUDED_BAZ_NON_BLOCKER_H
#define INCLUDED_BAZ_NON_BLOCKER_H

#include <gnuradio/sync_block.h>
//#include <gnuradio/msg_queue.h>

class BAZ_API baz_non_blocker;

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
typedef boost::shared_ptr<baz_non_blocker> baz_non_blocker_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_non_blocker.
 *
 * To avoid accidental use of raw pointers, baz_non_blocker's
 * constructor is private.  howto_make_square2_ff is the public
 * interface for creating new instances.
 */
BAZ_API baz_non_blocker_sptr baz_make_non_blocker (int item_size, /*gr::msg_queue::sptr queue, */bool blocking = false);

/*!
 * \brief square2 a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr::sync_block.
 */
class BAZ_API baz_non_blocker : public gr::block
{
private:
  // The friend declaration allows howto_make_square2_ff to
  // access the private constructor.

  friend BAZ_API baz_non_blocker_sptr baz_make_non_blocker (int item_size, /*gr::msg_queue::sptr queue, */bool blocking);

  baz_non_blocker (int item_size, /*gr::msg_queue::sptr queue, */bool blocking);  	// private constructor

  int d_item_size;
  //gr::msg_queue::sptr d_queue;
  bool d_blocking, d_blocking_forecasted;

public:
  ~baz_non_blocker ();	// public destructor

  int general_work (int noutput_items, gr_vector_int &ninput_items,
	    gr_vector_const_void_star &input_items,
	    gr_vector_void_star &output_items);

  void forecast(int noutput_items, gr_vector_int &ninput_items_required);

  void set_blocking(bool enable = true);
};

#endif /* INCLUDED_BAZ_NATIVE_MUX_H */
