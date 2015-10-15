/* -*- c++ -*- */
/*
 * Copyright 2008,2010,2012 Free Software Foundation, Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "baz_additive_scrambler_bb.h"
#include <gnuradio/io_signature.h>
//#include <gnuradio/digital/lfsr.h>

namespace gr {
namespace baz {

    class lfsr
    {
    private:
      uint32_t d_shift_register;
      uint32_t d_mask;
      uint32_t d_seed;
      uint32_t d_shift_register_length; // less than 32

      static uint32_t
      popCount(uint32_t x)
      {
        uint32_t r = x - ((x >> 1) & 033333333333)
          - ((x >> 2) & 011111111111);
        return ((r + (r >> 3)) & 030707070707) % 63;
      }

    public:
      lfsr(uint32_t mask, uint32_t seed, uint32_t reg_len)
      : d_shift_register(seed),
      d_mask(mask),
      d_seed(seed),
      d_shift_register_length(reg_len)
      {
        if(reg_len > 31)
          throw std::invalid_argument("reg_len must be <= 31");
      }

      unsigned char next_bit(bool return_newbit = false)
      {
        unsigned char output = d_shift_register & 1;
        unsigned char newbit = popCount( d_shift_register & d_mask )%2;
        d_shift_register = ((d_shift_register>>1) | (newbit<<d_shift_register_length));
        return (return_newbit ? newbit : output);
      }

      unsigned char next_bit_scramble(unsigned char input)
      {
        unsigned char output = d_shift_register & 1;
        unsigned char newbit = (popCount( d_shift_register & d_mask )%2)^(input & 1);
        d_shift_register = ((d_shift_register>>1) | (newbit<<d_shift_register_length));
        return output;
      }

      unsigned char next_bit_descramble(unsigned char input)
      {
        unsigned char output = (popCount( d_shift_register & d_mask )%2)^(input & 1);
        unsigned char newbit = input & 1;
        d_shift_register = ((d_shift_register>>1) | (newbit<<d_shift_register_length));
        return output;
      }

      /*!
       * Reset shift register to initial seed value
       */
      void reset() { d_shift_register = d_seed; }

      /*!
       * Rotate the register through x number of bits
       * where we are just throwing away the results to get queued up correctly
       */
      void pre_shift(int num)
      {
        for(int i=0; i<num; i++) {
          next_bit();
        }
      }

      int mask() const { return d_mask; }
    };

  /////////////////////////////////////////////////////////////////////////////

    class additive_scrambler_bb_impl
      : public additive_scrambler_bb
    {
    private:
      baz::lfsr d_lfsr;
      int      d_count; //!< Reset the LFSR after this many bytes (not bits)
      int      d_bytes; //!< Count the processed bytes
      int      d_len;
      int      d_seed;
      int      d_bits_per_byte;
      pmt::pmt_t d_reset_tag_key; //!< Reset the LFSR when this tag is received

      int _get_next_reset_index(int noutput_items, int last_reset_index=-1);

    public:
      additive_scrambler_bb_impl(int mask, int seed,
         int len, int count=0,
         int bits_per_byte=1, const std::string &reset_tag_key="");
      ~additive_scrambler_bb_impl();

      int mask() const;
      int seed() const;
      int len() const;
      int count() const;
      int bits_per_byte() { return d_bits_per_byte; };

      int work(int noutput_items,
         gr_vector_const_void_star &input_items,
         gr_vector_void_star &output_items);
    };

    ///////////////////////////////////////////////////////////////////////////

    additive_scrambler_bb::sptr
    additive_scrambler_bb::make (int mask, int seed,
				 int len, int count,
				 int bits_per_byte,
				 const std::string &reset_tag_key)
    {
      return gnuradio::get_initial_sptr(new additive_scrambler_bb_impl
					(mask, seed, len, count, bits_per_byte, reset_tag_key));
    }

    additive_scrambler_bb_impl::additive_scrambler_bb_impl(int mask,
							   int seed,
							   int len,
							   int count,
							   int bits_per_byte,
							   const std::string &reset_tag_key)
      : sync_block("additive_scrambler_bb",
		      gr::io_signature::make(1, 1, sizeof(unsigned char)),
		      gr::io_signature::make(1, 1, sizeof(unsigned char))),
	d_lfsr(mask, seed, len),
	d_count(reset_tag_key.empty() ? count : -1),
	d_bytes(0), d_len(len), d_seed(seed),
	d_bits_per_byte(bits_per_byte), d_reset_tag_key(pmt::string_to_symbol(reset_tag_key))
    {
      if (d_count < -1) {
      	throw std::invalid_argument("count must be non-negative!");
      }
      if (d_bits_per_byte < 1 || d_bits_per_byte > 8) {
      	throw std::invalid_argument("bits_per_byte must be in [1, 8]");
      }
    }

    additive_scrambler_bb_impl::~additive_scrambler_bb_impl()
    {
    }

    int
    additive_scrambler_bb_impl::mask() const 
    { 
      return d_lfsr.mask();
    }

    int
    additive_scrambler_bb_impl::seed() const
    {
      return d_seed;
    }

    int
    additive_scrambler_bb_impl::len() const
    {
      return d_len;
    }

    int
    additive_scrambler_bb_impl::count() const
    {
      return d_count;
    }

    int additive_scrambler_bb_impl::_get_next_reset_index(int noutput_items, int last_reset_index /* = -1 */)
    {
      int reset_index = noutput_items; // This is a value that gets never reached in the for loop
      if (d_count == -1) {
	std::vector<gr::tag_t> tags;
	get_tags_in_range(tags, 0, nitems_read(0), nitems_read(0)+noutput_items, d_reset_tag_key);
	for (unsigned i = 0; i < tags.size(); i++) {
	  int reset_pos = tags[i].offset - nitems_read(0);
	  if (reset_pos < reset_index && reset_pos > last_reset_index) {
	    reset_index = reset_pos;
	  };
	}
      } else {
	if (last_reset_index == -1) {
	  reset_index = d_count - d_bytes;
	} else {
	  reset_index = last_reset_index + d_count;
	}
      }
      return reset_index;
    }

    int
    additive_scrambler_bb_impl::work(int noutput_items,
				     gr_vector_const_void_star &input_items,
				     gr_vector_void_star &output_items)
    {
      const unsigned char *in = (const unsigned char *)input_items[0];
      unsigned char *out = (unsigned char *)output_items[0];
      int reset_index = _get_next_reset_index(noutput_items);

      for(int i = 0; i < noutput_items; i++) {
          if (1 && i == reset_index) {
              //fprintf(stderr, "Resetting LFSR at index %d\n", i);
              d_lfsr.reset();
              d_bytes = 0;
              reset_index = _get_next_reset_index(noutput_items, reset_index);
          }
	unsigned char scramble_byte = 0x00;
	for (int k = 0; k < d_bits_per_byte; k++) {
	  scramble_byte ^= (d_lfsr.next_bit(true) << k);
	}
	out[i] = in[i] ^ scramble_byte;
	d_bytes++;
	if (0 && i == reset_index) {
        fprintf(stderr, "Resetting LFSR at index %d\n", i);
	  d_lfsr.reset();
	  d_bytes = 0;
	  reset_index = _get_next_reset_index(noutput_items, reset_index);
	}
      }

      return noutput_items;
    }

} /* namespace baz */
} /* namespace gr */
