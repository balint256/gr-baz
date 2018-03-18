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

#include "baz_colouriser.h"
#include <gnuradio/io_signature.h>

#include "baz_colouriser_gradient.h"

#include <cstdio>
#include "stdio.h"

namespace gr {
  namespace baz {

    class colouriser_impl : public colouriser
    {
    private:
      bool d_verbose;
      float d_ref_lvl, d_dyn_rng;
      std::vector<uint32_t> d_lut;
      int d_vlen_in;
      static const int BPP = 4;

    public:
      colouriser_impl(float ref_lvl = 1.0, float dyn_rng = 1.0, int vlen_in = 1, bool verbose = false);
      ~colouriser_impl();

      // FIXME: If verbose, print values on change
      void set_dyn_rng(float dyn_rng) { d_dyn_rng = dyn_rng; }
      void set_ref_lvl(float ref_lvl) { d_ref_lvl = ref_lvl; }

      //float gain() const { return d_gain; }

      //int work(int noutput_items,
      //     gr_vector_const_void_star &input_items,
      //     gr_vector_void_star &output_items);

      void forecast(int noutput_items, gr_vector_int &ninput_items_required);
      int general_work(int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items);
    };

    colouriser::sptr
    colouriser::make(float ref_lvl, float dyn_rng, int vlen_in, bool verbose)
    {
      return gnuradio::get_initial_sptr(new colouriser_impl(ref_lvl, dyn_rng, vlen_in, verbose));
    }

    colouriser_impl::colouriser_impl(float ref_lvl, float dyn_rng, int vlen_in, bool verbose)
      : block("colouriser",
              io_signature::make(1, 1, (sizeof(float) * vlen_in)),
              io_signature::make(1, 1, sizeof(char)))
    , d_ref_lvl(ref_lvl)
    , d_dyn_rng(dyn_rng)
    , d_vlen_in(vlen_in)
    , d_verbose(verbose)
    {
      set_output_multiple(BPP);

      size_t gradient_len = sizeof(gradient) / sizeof(gradient[0]);

      for (size_t i = 0; i < gradient_len; ++i)
      {
        unsigned int c = gradient[i];
        unsigned char _a = c >> 24;
        unsigned char _b = (c >> 16) & 0xFF;
        unsigned char _g = (c >> 8) & 0xFF;
        unsigned char _r = c & 0xFF;

        /*float r = _r;
        float g = _g;
        float b = _b;

        float _y = (0.299 * r) + (0.587 * g) + (0.114 * b);
        float _u = (-0.147 * r) - (0.289 * g) + (0.436 * b);
        float _v = (0.615 * r) - (0.515 * g) - (0.100 * b);

        _y = std::min(std::max(_y, 0.0f), 255.0f);
        _u = std::min(std::max(_u, 0.0f), 255.0f);
        _v = std::min(std::max(_v, 0.0f), 255.0f);

        unsigned char y = (unsigned char)_y;
        unsigned char u = (unsigned char)_u;
        unsigned char v = (unsigned char)_v;

        //uint32_t vuy = v << 16 | u << 8 | y;

        y = (unsigned char)(255.0 * ((float)i / (float)gradient_len));
        uint32_t vuy = y;

        d_lut.push_back(vuy);*/

        //uint32_t rgba = _r << 24 | _g << 16 | _b << 8 | _a;
        //uint32_t rgba = _b << 24 | _g << 16 | _r << 8 | _a;

        //d_lut.push_back(rgba);

        d_lut.push_back(c);
      }

      fprintf(stderr, "[%s<%ld>] ref level: %f, dyn range: %f, vlen_in: %d, verbose: %s, gradient size: %lu\n", name().c_str(), unique_id(), ref_lvl, dyn_rng, vlen_in, (verbose ? "yes" : "no"), d_lut.size());

      set_relative_rate((double)(BPP * vlen_in));
    }

    colouriser_impl::~colouriser_impl()
    {
    }

    void colouriser_impl::forecast(int noutput_items, gr_vector_int &ninput_items_required)
    {
      //if (d_verbose) fprintf(stderr, "[%s<%ld>] forecast: %d\n", name().c_str(), unique_id(), noutput_items);

      for (int i = 0; i < ninput_items_required.size(); ++i)
      {
        ninput_items_required[i] = (int)ceil((double)noutput_items / relative_rate());
        //if (ninput_items_required[i] == 0)
        //  fprintf(stderr, "[%s<%ld>] Zero forecast!\n", name().c_str(), unique_id());
      }
    }

    //int colouriser_impl::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    int colouriser_impl::general_work(int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    {
      const float *iptr = (const float*)input_items[0];

      char *optr = (char*)output_items[0];

      //assert(noutput_items <= (ninput_items[0] * BPP));

      if (((ninput_items[0] * d_vlen_in) * BPP) < noutput_items)
      {
        fprintf(stderr, "[%s<%ld>] Too few items!\n", name().c_str(), unique_id());

        return -1;
      }

      int rounded_noutput_items = ((noutput_items / BPP) / d_vlen_in) * d_vlen_in * BPP;

      if (rounded_noutput_items == 0) // FIXME: This is happening all the time?!
      {
        //fprintf(stderr, "[%s<%ld>] Too few input vectors!\n", name().c_str(), unique_id());

        return 0;
      }

      ///*if (d_verbose) */fprintf(stderr, "[%s<%ld>] Work: out %d, in %d\n", name().c_str(), unique_id(), noutput_items, ninput_items[0]);

      int i = 0;
      for (; i < (rounded_noutput_items / BPP); i++)
      {
        int idx = (int)((iptr[i] - (d_ref_lvl - d_dyn_rng)) * (float)d_lut.size() / d_dyn_rng);

        if (idx < 0)
          idx = 0;
        else if (idx >= d_lut.size())
          idx = d_lut.size() - 1;

        memcpy(optr + (i * BPP), &d_lut[idx], BPP);
      }

      consume(0, i / d_vlen_in);  // Will divide due to rounding above

      return rounded_noutput_items;
    }

  } /* namespace baz */
} /* namespace gr */
