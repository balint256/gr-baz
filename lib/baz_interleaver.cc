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

//#undef NDEBUG

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "baz_interleaver.h"
#include <gnuradio/io_signature.h>

#include <cstdio>
#include "stdio.h"

namespace gr {
  namespace baz {

    class interleaver_impl : public interleaver
    {
    private:
      int d_item_size;
      int d_vlen_in, d_vlen_out;
      int d_out_trigger;
      int d_output_ports;
      bool d_top_down_in;
      bool d_vector_in, d_vector_out;
      bool d_verbose;

      bool d_reading_out;
      int d_trigger_idx;
      int d_current_col;
      size_t d_read_out_count;

    public:
      interleaver_impl(int item_size, int vlen_in, int vlen_out, int out_trigger = 0, int output_ports = 1, bool top_down_in = false, bool vector_in = true, bool vector_out = true, bool verbose = false);
      ~interleaver_impl();

      //void set_swap(bool swap) { d_swap = swap; }

      //bool get_swap() const { return d_swap; }

      //int work(int noutput_items,
      //     gr_vector_const_void_star &input_items,
      //     gr_vector_void_star &output_items);

      void forecast(int noutput_items, gr_vector_int &ninput_items_required);
      int general_work(int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items);
    };

    interleaver::sptr
    interleaver::make(int item_size, int vlen_in, int vlen_out, int out_trigger /*= 0*/, int output_ports /*= 1*/, bool top_down_in /*= false*/, bool vector_in /*= true*/, bool vector_out /*= true*/, bool verbose /*= false*/)
    {
      return gnuradio::get_initial_sptr(new interleaver_impl(item_size, vlen_in, vlen_out, out_trigger, output_ports, top_down_in, vector_in, vector_out, verbose));
    }

    interleaver_impl::interleaver_impl(int item_size, int vlen_in, int vlen_out, int out_trigger /*= 0*/, int output_ports /*= 1*/, bool top_down_in /*= false*/, bool vector_in /*= true*/, bool vector_out /*= true*/, bool verbose /*= false*/)
      : block("interleaver",
              io_signature::make(1, 1, (vector_in ? (item_size * vlen_in) : item_size)),
              io_signature::make(output_ports, output_ports, (vector_out ? (item_size * vlen_out) : item_size)))
    , d_item_size(item_size)
    , d_vlen_in(vlen_in)
    , d_vlen_out(vlen_out)
    , d_out_trigger(out_trigger * vlen_in)  // Convert to samples
    , d_output_ports(output_ports)
    , d_top_down_in(top_down_in)
    , d_vector_in(vector_in)
    , d_vector_out(vector_out)
    , d_verbose(verbose)
    , d_reading_out(false)
    , d_trigger_idx(0)
    , d_current_col(0)
    , d_read_out_count(0+1)
    {
      if (out_trigger <= 0)
        d_out_trigger = vlen_out * vlen_in; // Trigger after this many In Vectors (rows). By default all rows. Convert to samples.

      if (vector_out == false)
        set_output_multiple(vlen_out);  // One Out Vector at a time

      // If top_down, read back through history (buffer R->L). If not top_down (bottom up), read forward through history (buffer L->R).
      set_history(vlen_out * (vector_in ? 1 : vlen_in));

      // d_out_trigger: after how many [rows] (samples) in do we read all the columns out
      //d_out_trigger;
      //set_relative_rate();

      fprintf(stderr, "[%s<%ld>] item size: %d, vlen_in: %d, vlen_out: %d, out trigger: %d (%d samples), output ports: %d, top-down in: %s, vector in: %s, vector out: %s, verbose: %s\n", name().c_str(), unique_id(), 
        item_size, vlen_in, vlen_out, out_trigger, d_out_trigger, output_ports, (top_down_in ? "yes" : "no"), (vector_in ? "yes" : "no"), (vector_out ? "yes" : "no"), (verbose ? "yes" : "no"));
    }

    interleaver_impl::~interleaver_impl()
    {
    }

    void interleaver_impl::forecast(int noutput_items, gr_vector_int &ninput_items_required)
    {
      //if (d_verbose) fprintf(stderr, "[%s<%ld>] forecast: %d\n", name().c_str(), unique_id(), noutput_items);

      for (int i = 0; i < ninput_items_required.size(); ++i)
      {
        if (d_reading_out)
        {
          ninput_items_required[i] = 0;
        }
        else
        {
          //int n = noutput_items * (d_vector_out ? d_vlen_out : 1); // Samples out
          ////n /= d_vlen_out;  // Out Vectors out
          //n *= d_output_ports; // Will be split across output ports
          //ninput_items_required[i] = (int)ceil((double)n / (d_vector_in ? (double)d_vlen_in : 1));
          //continue;

          int out_trigger = ((d_read_out_count == 0) ? (d_vlen_out * d_vlen_in) : d_out_trigger); // Samples

          int diff = out_trigger - d_trigger_idx;

          assert((diff > 0) && (diff <= out_trigger));

          // # items to be able to next read-out cycle (doesn't depend on 'noutput_items')
          if (d_vector_in == false)
            ninput_items_required[i] = diff;
          else
            ninput_items_required[i] = (int)ceil((double)diff / (double)d_vlen_in);
        }
      }
    }

    //int interleaver_impl::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    int interleaver_impl::general_work(int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    {
      if (d_reading_out)
      {
        const char *iptr = (const char*)input_items[0];

        int n = noutput_items * (d_vector_out ? d_vlen_out : 1); // Samples out
        n /= d_vlen_out;  // Out Vectors out (round down)

        if (n == 0)
        {
          /*if (d_verbose) */fprintf(stderr, "[%s<%ld>] Not enough of an Out Vector to read out! (only %d output items)\n", name().c_str(), unique_id(), noutput_items);

          return -1;
        }

        // Assuming 'noutput_items' will automatically apply to all output ports
        assert(d_output_ports == output_items.size());
        n *= output_items.size();  // Need to loop for as many output ports
        //assert((n % noutput_items.size()) == 0);

        //if (d_verbose) fprintf(stderr, "[%s<%ld>] Reading out %d columns (current idx: %d)\n", name().c_str(), unique_id(), n, d_current_col);

        int output_idx = 0; // Which output port this column will be written to (should always end up going through each once in one work iteration)
        size_t col = 0;
        for (; col < n; ++col)
        {
          char *optr = (char*)output_items[output_idx];

          // FIXME: Options to read row in reverse/write to column in reverse

          if (d_top_down_in)
          {
            for (size_t i = 0; i < d_vlen_out; ++i)
            {
              memcpy(optr + ((((col / output_items.size()) * d_vlen_out) + i) * d_item_size), iptr + (((((d_vlen_out - 1) - i) * d_vlen_in) + d_current_col) * d_item_size), d_item_size);
            }
          }
          else
          {
            for (size_t i = 0; i < d_vlen_out; ++i)
            {
              memcpy(optr + ((((col / output_items.size()) * d_vlen_out) + i) * d_item_size), iptr + (((i * d_vlen_in) + d_current_col) * d_item_size), d_item_size);
            }
          }

          output_idx = (output_idx + 1) % output_items.size();

          d_current_col += 1;

          if (d_current_col == d_vlen_in)
          {
            ++col;  // We're about to break out

            d_current_col = 0;
            d_reading_out = false;

            //assert(col == (n-1)); // This won't always be true

            assert(d_trigger_idx == 0);

            if (d_verbose) fprintf(stderr, "[%s<%ld>] #%04lu Switched to read-in\n", name().c_str(), unique_id(), d_read_out_count);

            ++d_read_out_count;

            break;
          }
        }

        assert(output_idx == 0); // Should have come full circle

        assert((col % output_items.size()) == 0);

        return ((col / output_items.size()) * (d_vector_out ? 1 : d_vlen_out));
      }
      else
      {
        int i = ninput_items[0] * (d_vector_in ? d_vlen_in : 1);  // Samples in
        //i /= d_vlen_in; // In Vectors in (rounded down)

        if (i == 0)
        {
          /*if (d_verbose) */fprintf(stderr, "[%s<%ld>] Not enough of an In Vector to read in! (only %d input items)\n", name().c_str(), unique_id(), ninput_items[0]);

          return -1;
        }

        int out_trigger = ((d_read_out_count == 0) ? (d_vlen_out * d_vlen_in) : d_out_trigger);

        int diff = out_trigger - d_trigger_idx;

        assert((diff > 0) && (diff <= out_trigger));

        int to_consume = std::min(diff, i); // So we can complete the next read-out cycle

        if (d_verbose) fprintf(stderr, "[%s<%ld>] #%04lu Work: i: %d, noutput_items: %d, ninput_items: %d, out_trigger: %d, diff: %d, to_consume: %d, trigger_idx: %d\n", 
          name().c_str(), unique_id(), d_read_out_count, i, noutput_items, ninput_items[0], out_trigger, diff, to_consume, d_trigger_idx);

        assert((d_vector_in == false) || ((to_consume % d_vlen_in) == 0));

        //consume(0, (to_consume * (d_vector_in ? 1 : d_vlen_in)));
        consume(0, (to_consume / (d_vector_in ? d_vlen_in : 1)));

        d_trigger_idx += to_consume;

        if (d_verbose) fprintf(stderr, "[%s<%ld>] #%04lu Trigger idx now: %d\n", name().c_str(), unique_id(), d_read_out_count, d_trigger_idx);

        assert(d_trigger_idx <= out_trigger);

        if (d_trigger_idx == out_trigger)
        {
          assert(d_current_col == 0); // Should have been reset at end of last read-out

          d_trigger_idx = 0;
          d_reading_out = true;

          if (d_verbose) fprintf(stderr, "[%s<%ld>] #%04lu Switched to read-out\n", name().c_str(), unique_id(), d_read_out_count);

          //++d_read_out_count;
        }
      }

      return 0;
    }

  } /* namespace baz */
} /* namespace gr */
