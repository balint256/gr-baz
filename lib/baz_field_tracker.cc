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

#include "baz_field_tracker.h"
#include <gnuradio/io_signature.h>

#include <cstdio>
#include "stdio.h"

namespace gr {
  namespace baz {

    class field_tracker_impl : public field_tracker
    {
    private:
      int d_item_size;
      int d_input_length;
      int d_output_length;
      // int d_pad_length;

      bool d_odd;
      int d_padding;
      int d_to_copy;
      // bool d_residual;

    public:
      field_tracker_impl(int item_size, int input_length, int output_length/*, int pad_length*/);
      ~field_tracker_impl();

      // void set_swap(bool swap) { d_swap = swap; }

      // bool get_swap() const { return d_swap; }

      // int work(int noutput_items,
      //      gr_vector_const_void_star &input_items,
      //      gr_vector_void_star &output_items);

      void forecast(int noutput_items, gr_vector_int &ninput_items_required);

      int general_work(int noutput_items,
                      gr_vector_int &ninput_items,
                      gr_vector_const_void_star &input_items,
                      gr_vector_void_star &output_items);
    };

    field_tracker::sptr
    field_tracker::make(int item_size, int input_length, int output_length/*, int pad_length*/)
    {
      return gnuradio::get_initial_sptr(new field_tracker_impl(item_size, input_length, output_length/*, pad_length*/));
    }

    field_tracker_impl::field_tracker_impl(int item_size, int input_length, int output_length/*, int pad_length*/)  // FIXME: item_size
      : block("field_tracker",
              io_signature::make(3, 3, item_size), // signal[, trigger], even corr, odd corr
              io_signature::make(1, 1, item_size))
    , d_item_size(item_size)
    , d_input_length(input_length)
    , d_output_length(output_length)
    // , d_pad_length(pad_length)
    , d_odd(true)
    , d_padding(0)
    , d_to_copy(0)
    // , d_residual(false)
    {
      fprintf(stderr, "[%s<%ld>] item size: %d, input length: %d, output length: %d\n", name().c_str(), unique_id(), item_size, input_length, output_length/*, pad_length*/); //, pad length: %d
    }

    field_tracker_impl::~field_tracker_impl()
    {
    }

    void field_tracker_impl::forecast(int noutput_items, gr_vector_int &ninput_items_required)
    {
      //if (d_verbose) fprintf(stderr, "[%s<%ld>] forecast: %d\n", name().c_str(), unique_id(), noutput_items);

      for (int i = 0; i < ninput_items_required.size(); ++i)
      {
        ninput_items_required[i] = ((d_padding == 0) ? noutput_items : 0);
      }
    }

    // int swap_impl::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    int field_tracker_impl::general_work(int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    {
      const float *iptr    = (const float*)input_items[0];
      // const float *trigger = (const float*)input_items[1];
      const float *even    = (const float*)input_items[1];
      const float *odd     = (const float*)input_items[2];
      float *optr = (float*)output_items[0];

      ////////////////////////////

      int i = 0;
      int j = 0;

      for (; j < noutput_items;)
      {
        if (d_to_copy > 0)
        {
          int to_copy = std::min((noutput_items - j), d_to_copy);

          memcpy(optr + j, iptr + i, to_copy * d_item_size);

          i += to_copy;
          j += to_copy;

          d_to_copy -= to_copy;

          if (d_to_copy == 0)
          {
            if (d_odd == false)
            {
              d_padding = d_output_length - d_input_length;
            }
          }

          continue;
        }

        if (d_padding > 0)
        {
          int to_zero = std::min((noutput_items - j), d_padding);

          memset(optr + j, 0x00, to_zero * d_item_size);

          j += to_zero;

          d_padding -= to_zero;

          if (d_padding == 0)
          {
            if (d_odd)
            {
              d_to_copy = d_input_length;
            }
          }

          break;  // FIXME: Due to forecast and no input items available (check ninput_items above)
        }

        int input_offset = (nitems_read(0) + i) % d_input_length;
        int output_offset = (nitems_written(0) + j) % d_output_length;

        // if (input_offset == 0)
        {
          bool is_even = (even[i] >= odd[i]);

          if (is_even)
          {
            // fprintf(stderr, "[%s<%ld>] Even %d\n", name().c_str(), unique_id(), input_offset);

            // Copy d_input_length
            d_to_copy = d_input_length;
            // Zero (d_output_length - d_input_length)
            if (d_odd == false)
            {
              // fprintf(stderr, "[%s<%ld>] Was even!\n", name().c_str(), unique_id());
            }

            d_odd = false;
          }
          else
          {
            // fprintf(stderr, "[%s<%ld>] Odd %d\n", name().c_str(), unique_id(), input_offset);

            // Zero (d_output_length - d_input_length)
            if (d_odd)
            {
              // fprintf(stderr, "[%s<%ld>] Was odd!\n", name().c_str(), unique_id());
            }
            d_odd = true;
            d_padding = d_output_length - d_input_length;
            // Copy d_input_length
          }
        }
      }

      consume_each(i);

      ////////////////////////////
/*
      if (d_padding >= noutput_items)
      {
        consume_each(noutput_items);
        d_padding -= noutput_items;
        return 0;
      }

      int start = d_padding;
      d_padding = 0;

      int j = 0;

      for (int i = start; i < noutput_items; ++i)
      {
        optr[j++] = iptr[i];

        // if (d_padding == 0)
        // {
          if (trigger[i] > 0.0f)
          {
            if (even[i] >= odd[i])
            {
              // Continue as normal

              d_odd = false;

              fprintf(stderr, "[%s<%ld>] Even\n", name().c_str(), unique_id());
            }
            else
            {
              d_odd = true;

              fprintf(stderr, "[%s<%ld>] Odd\n", name().c_str(), unique_id());

              d_padding = d_pad_length;

              int to_pad = std::min((noutput_items - i), d_padding);

              i += to_pad;

              d_padding -= to_pad;
            }
          }
          else
          {
            // optr[j++] = iptr[i];
          }
        // }
      }
      
      // memcpy(optr, iptr, d_item_size * noutput_items);

      consume_each(noutput_items);
*/
      return j;
    }

  } /* namespace baz */
} /* namespace gr */
