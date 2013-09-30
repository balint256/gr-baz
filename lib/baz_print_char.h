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

#ifndef INCLUDED_BAZ_PRINT_CHAR_H
#define INCLUDED_BAZ_PRINT_CHAR_H

#include <gnuradio/sync_block.h>

class BAZ_API baz_print_char;

/*
 * See http://www.boost.org/libs/smart_ptr/smart_ptr.htm
 */
typedef boost::shared_ptr<baz_print_char> baz_print_char_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_print_char.
 *
 * To avoid accidental use of raw pointers, baz_print_char's
 * constructor is private.  howto_make_square_ff is the public
 * interface for creating new instances.
 */
BAZ_API baz_print_char_sptr baz_make_print_char (float threshold = 0.0, int limit = -1, const char* file = NULL);

/*!
 * \brief square a stream of floats.
 * \ingroup block
 *
 * \sa howto_square2_ff for a version that subclasses gr::sync_block.
 */
class BAZ_API baz_print_char : public gr::sync_block
{
private:
  friend BAZ_API baz_print_char_sptr baz_make_print_char (float threshold, int limit, const char* file);
  baz_print_char (float threshold, int limit, const char* file);  	// private constructor
private:
	float d_threshold;
	int d_limit;
	int d_length;
	FILE* d_file;
 public:
  ~baz_print_char ();	// public destructor
  //int general_work (int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
  int work (int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_PRINT_CHAR_H */
