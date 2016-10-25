/* -*- c++ -*- */
/*
 * Copyright 2007,2009,2010 Free Software Foundation, Inc.
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

#include "baz_swap.h"
#include <gnuradio/io_signature.h>

#include <cstdio>
#include "stdio.h"

namespace gr {
  namespace baz {

    class swap_impl : public swap
    {
    private:
      int d_item_size, d_vlen;
      bool d_swap;

    public:
      swap_impl(int item_size, int vlen, bool swap = true);
      ~swap_impl();

      void set_swap(bool swap) { d_swap = swap; }

      bool get_swap() const { return d_swap; }

      int work(int noutput_items,
           gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);

      //void forecast(int noutput_items, gr_vector_int &ninput_items_required);
      //int general_work(int noutput_items,
      //                 gr_vector_int &ninput_items,
      //                 gr_vector_const_void_star &input_items,
      //                 gr_vector_void_star &output_items);
    };

    swap::sptr
    swap::make(int item_size, int vlen, bool swap)
    {
      return gnuradio::get_initial_sptr(new swap_impl(item_size, vlen, swap));
    }

    swap_impl::swap_impl(int item_size, int vlen, bool swap)
      : sync_block("swap",
              io_signature::make(1, 1, item_size),
              io_signature::make(1, 1, item_size))
    , d_item_size(item_size)
    , d_vlen(vlen)
    , d_swap(swap)
    {
      set_output_multiple(2 * vlen);

      fprintf(stderr, "[%s<%ld>] item size: %d, vlen: %d, swap: %s\n", name().c_str(), unique_id(), item_size, vlen, (swap ? "yes" : "no"));
    }

    swap_impl::~swap_impl()
    {
    }

    /*void swap_impl::forecast(int noutput_items, gr_vector_int &ninput_items_required)
    {
      //if (d_verbose) fprintf(stderr, "[%s<%ld>] forecast: %d\n", name().c_str(), unique_id(), noutput_items);

      for (int i = 0; i < ninput_items_required.size(); ++i)
        ninput_items_required[i] = (int)ceil((float)noutput_items / relative_rate());
    }*/

    int swap_impl::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    //int swap_impl::general_work(int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    {
      const char *iptr = (const char*)input_items[0];
      char *optr = (char*)output_items[0];

      size_t item = d_item_size * d_vlen;

      if (d_swap)
      {
        for (int i = 0; i < (noutput_items / (2 * d_vlen)); ++i)
        {
          memcpy(optr + (i * 2 + 0) * item, iptr + (i * 2 + 1) * item, item);
          memcpy(optr + (i * 2 + 1) * item, iptr + (i * 2 + 0) * item, item);
        }
      }
      else
      {
        memcpy(optr, iptr, d_item_size * noutput_items);
      }

      return noutput_items;
    }

  } /* namespace baz */
} /* namespace gr */
