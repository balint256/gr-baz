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

#ifndef INCLUDED_BAZ_AUTO_BER_BF_H
#define INCLUDED_BAZ_AUTO_BER_BF_H

#include <gnuradio/sync_block.h>

//#include <hash_map>
#include <boost/unordered_map.hpp>
#include <vector>

class BAZ_API baz_auto_ber_bf;

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
typedef boost::shared_ptr<baz_auto_ber_bf> baz_auto_ber_bf_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_auto_ber_bf.
 *
 * To avoid accidental use of raw pointers, baz_auto_ber_bf's
 * constructor is private.  howto_make_square2_ff is the public
 * interface for creating new instances.
 */
BAZ_API baz_auto_ber_bf_sptr baz_make_auto_ber_bf (int degree, int sync_bits, int sync_decim/*, int sync_skip*/);

namespace gr { namespace digital {
class glfsr;
} }

/*!
 * \brief square2 a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr::sync_block.
 */
class BAZ_API baz_auto_ber_bf : public gr::sync_block
{
private:
	// The friend declaration allows howto_make_square2_ff to
	// access the private constructor.

	friend BAZ_API baz_auto_ber_bf_sptr baz_make_auto_ber_bf (int degree, int sync_bits, int sync_decim);

	baz_auto_ber_bf (int degree, int sync_bits, int sync_decim);  	// private constructor

	gr::digital::glfsr* d_glfsr;
	int d_glfsr_length, d_glfsr_rounded_length;
	//typedef std::hash_map<uint64_t, int> SyncMap;
	typedef boost::unordered_map<uint64_t, int> SyncMap;
	SyncMap d_sync_map, d_dupe_map;
	std::vector<uint64_t> d_sync_list;
	uint64_t d_current_word;
	int d_sync_bit_length;

public:
	~baz_auto_ber_bf ();	// public destructor

//	void set_exponent(float exponent);

//	inline float exponent() const
//	{ return d_exponent; }

	int work (int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_AUTO_BER_BF_H */
