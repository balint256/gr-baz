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

#ifndef INCLUDED_BAZ_BLOCK_STATUS_H
#define INCLUDED_BAZ_BLOCK_STATUS_H

#include <gr_sync_block.h>
#include <gr_msg_queue.h>

class baz_block_status;

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
typedef boost::shared_ptr<baz_block_status> baz_block_status_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_block_status.
 *
 * To avoid accidental use of raw pointers, baz_block_status's
 * constructor is private.  howto_make_square2_ff is the public
 * interface for creating new instances.
 */
baz_block_status_sptr baz_make_block_status (int size, gr_msg_queue_sptr queue, unsigned long work_iterations, unsigned long samples_processed);

/*!
 * \brief square2 a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr_sync_block.
 */
class baz_block_status : public gr_sync_block
{
private:
  // The friend declaration allows howto_make_square2_ff to
  // access the private constructor.

  friend baz_block_status_sptr baz_make_block_status (int size, gr_msg_queue_sptr queue, unsigned long work_iterations, unsigned long samples_processed);

  baz_block_status (int size, gr_msg_queue_sptr queue, unsigned long work_iterations, unsigned long samples_processed);  	// private constructor

  int d_size;
  gr_msg_queue_sptr d_queue;
  unsigned long d_work_iterations;
  unsigned long d_samples_processed;

 public:
  ~baz_block_status ();	// public destructor

  void set_size(int size);

  inline int size() const
  { return d_size; }

  int work (int noutput_items,
	    gr_vector_const_void_star &input_items,
	    gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_BLOCK_STATUS_H */
