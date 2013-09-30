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

#ifndef INCLUDED_BAZ_PUNCTURE_BB_H
#define INCLUDED_BAZ_PUNCTURE_BB_H

#include <gnuradio/block.h>
#include <boost/thread.hpp>

class BAZ_API baz_puncture_bb;

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
typedef boost::shared_ptr<baz_puncture_bb> baz_puncture_bb_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_puncture_bb.
 *
 * To avoid accidental use of raw pointers, baz_puncture_bb's
 * constructor is private.  baz_make_puncture_bb is the public
 * interface for creating new instances.
 */
BAZ_API baz_puncture_bb_sptr baz_make_puncture_bb (const std::vector<int>& matrix);

/*!
 * \brief square a stream of floats.
 * \ingroup block
 *
 * \sa howto_square2_ff for a version that subclasses gr::sync_block.
 */
class BAZ_API baz_puncture_bb : public gr::block
{
private:
  // The friend declaration allows baz_make_puncture_bb to
  // access the private constructor.

  friend BAZ_API baz_puncture_bb_sptr baz_make_puncture_bb (const std::vector<int>& matrix);

  baz_puncture_bb (const std::vector<int>& matrix);  	// private constructor
  
  boost::mutex	d_mutex;
  char* m_pMatrix;
  int m_iLength;
  int m_iIndex;

 public:
  ~baz_puncture_bb ();	// public destructor

  void set_matrix(const std::vector<int>& matrix);
  
  void forecast(int noutput_items, gr_vector_int &ninput_items_required);

  int general_work (int noutput_items,
		    gr_vector_int &ninput_items,
		    gr_vector_const_void_star &input_items,
		    gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_PUNCTURE_BB_H */
