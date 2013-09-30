/* -*- c++ -*- */
/*
 * Copyright 2006,2013 Free Software Foundation, Inc.
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

#ifndef INCLUDED_BAZ_UNPACKED_TO_PACKED_BB_H
#define INCLUDED_BAZ_UNPACKED_TO_PACKED_BB_H

#include <gnuradio/block.h>
#include <gnuradio/endianness.h>

class BAZ_API baz_unpacked_to_packed_bb;
typedef boost::shared_ptr<baz_unpacked_to_packed_bb> baz_unpacked_to_packed_bb_sptr;

BAZ_API baz_unpacked_to_packed_bb_sptr
baz_make_unpacked_to_packed_bb (unsigned int bits_per_chunk, unsigned int bits_into_output, /*gr::endianness_t*/int endianness = gr::GR_MSB_FIRST);

/*!
 * \brief Convert a stream of unpacked bytes or shorts into a stream of packed bytes or shorts.
 * \ingroup converter_blk
 *
 * input: stream of unsigned char; output: stream of unsigned char
 *
 * This is the inverse of gr_packed_to_unpacked_XX.
 *
 * The low \p bits_per_chunk bits are extracted from each input byte or short.
 * These bits are then packed densely into the output bytes or shorts, such that
 * all 8 or 16 bits of the output bytes or shorts are filled with valid input bits.
 * The right thing is done if bits_per_chunk is not a power of two.
 *
 * The combination of gr_packed_to_unpacked_XX followed by
 * gr_chunks_to_symbols_Xf or gr_chunks_to_symbols_Xc handles the
 * general case of mapping from a stream of bytes or shorts into arbitrary float
 * or complex symbols.
 *
 * \sa gr_packed_to_unpacked_bb, gr_unpacked_to_packed_bb,
 * \sa gr_packed_to_unpacked_ss, gr_unpacked_to_packed_ss,
 * \sa gr_chunks_to_symbols_bf, gr_chunks_to_symbols_bc.
 * \sa gr_chunks_to_symbols_sf, gr_chunks_to_symbols_sc.
 */
class BAZ_API baz_unpacked_to_packed_bb : public gr::block
{
  friend BAZ_API baz_unpacked_to_packed_bb_sptr
  baz_make_unpacked_to_packed_bb (unsigned int bits_per_chunk, unsigned int bits_into_output, /*gr::endianness_t*/int endianness);

  baz_unpacked_to_packed_bb (unsigned int bits_per_chunk, unsigned int bits_into_output, /*gr::endianness_t*/int endianness);

  unsigned int    d_bits_per_chunk, d_bits_into_output;
  gr::endianness_t d_endianness;
  unsigned int    d_index;

 public:
  void forecast(int noutput_items, gr_vector_int &ninput_items_required);
  int general_work (int noutput_items,
		    gr_vector_int &ninput_items,
		    gr_vector_const_void_star &input_items,
		    gr_vector_void_star &output_items);

  bool check_topology(int ninputs, int noutputs) { return ninputs == noutputs; }
};

#endif
