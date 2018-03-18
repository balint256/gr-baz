/* -*- c++ -*- */
/*
 * Copyright 2004,2007,2010,2012-2013 Free Software Foundation, Inc.
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

#include <gnuradio/io_signature.h>
#include <gnuradio/filter/mmse_fir_interpolator_cc.h>
#include "baz_fractional_resampler_cc.h"
#include <stdexcept>

namespace gr {
  namespace baz {

    class BAZ_API fractional_resampler_cc_impl
      : public fractional_resampler_cc
    {
    private:
      long double d_mu;
      long double d_mu_inc;
      gr::filter::mmse_fir_interpolator_cc *d_resamp;
      volatile bool d_update;
      long double d_mu_inc_update;
      volatile bool d_update_mu;
      long double d_mu_update;
      volatile bool d_update_mu_adj;
      long double d_mu_adj;

    public:
      fractional_resampler_cc_impl(long double phase_shift,
                                   long double resamp_ratio,
                                   unsigned long long resamp_ratio_num = 0,
                                   unsigned long long resamp_ratio_denom = 0);
      ~fractional_resampler_cc_impl();

      void forecast(int noutput_items,
        gr_vector_int &ninput_items_required);
      int general_work(int noutput_items,
           gr_vector_int &ninput_items,
           gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);

      long double mu() const;
      long double resamp_ratio() const;
      void set_mu(long double mu);
      void set_resamp_ratio(long double resamp_ratio);
      void set_resamp_ratio(double resamp_ratio)
      { set_resamp_ratio((long double)resamp_ratio); }
      void set_resamp_ratio(unsigned long long resamp_ratio_num, unsigned long long resamp_ratio_denom);

      void handle_msg(pmt::pmt_t msg);
    };

    fractional_resampler_cc::sptr
    fractional_resampler_cc::make(/*long */double phase_shift, /*long */double resamp_ratio, unsigned long long resamp_ratio_num/* = 0*/, unsigned long long resamp_ratio_denom/* = 0*/)
    {
      return gnuradio::get_initial_sptr
        (new fractional_resampler_cc_impl(phase_shift, resamp_ratio, resamp_ratio_num, resamp_ratio_denom));
    }

    fractional_resampler_cc_impl::fractional_resampler_cc_impl
                                     (long double phase_shift, long double resamp_ratio, unsigned long long resamp_ratio_num/* = 0*/, unsigned long long resamp_ratio_denom/* = 0*/)
      : block("fractional_resampler_cc",
              io_signature::make2(1, 2, sizeof(gr_complex), sizeof(float)),
              io_signature::make(1, 1, sizeof(gr_complex))),
	d_mu(phase_shift), d_mu_inc(resamp_ratio),
	d_resamp(new gr::filter::mmse_fir_interpolator_cc()),
  d_update(false), d_update_mu(false), d_update_mu_adj(false)
    {
      if (resamp_ratio_denom != 0)
      {
        d_mu_inc = resamp_ratio = (long double)resamp_ratio_num / (long double)resamp_ratio_denom;
      }
      
      fprintf(stderr, "[%s<%ld>] Ratio: %.25Lf\n", name().c_str(), unique_id(), resamp_ratio);

      if(resamp_ratio <=  0)
	throw std::out_of_range("resampling ratio must be > 0");
      if(phase_shift <  0  || phase_shift > 1)
	throw std::out_of_range("phase shift ratio must be > 0 and < 1");

      set_relative_rate(1.0 / resamp_ratio);

      message_port_register_in(pmt::mp("msg"));
      set_msg_handler(pmt::mp("msg"), boost::bind(&fractional_resampler_cc_impl::handle_msg, this, _1));
    }

    fractional_resampler_cc_impl::~fractional_resampler_cc_impl()
    {
      delete d_resamp;
    }

    void fractional_resampler_cc_impl::handle_msg(pmt::pmt_t msg)
    {
      try
      {
        if (pmt::is_pair(msg))
        {
          pmt::pmt_t p1 = pmt::car(msg);
          pmt::pmt_t p2 = pmt::cdr(msg);
          long i = pmt::to_long(p1);
          double frac = pmt::to_double(p2);
          long double d = ((long double)i + (long double)frac) / (long double)1e9;  // ppb
          set_resamp_ratio(d);
        }
        else
        {
          double d = pmt::to_double(msg);
          // set_mu(d);

          d_mu_adj = (long double)d * d_mu_inc;
          d_update_mu_adj = true;
        }
      }
      catch (...)
      {
        fprintf(stderr, "Failed to handle PMT\n");
      }
    }

    void
    fractional_resampler_cc_impl::forecast(int noutput_items,
                                           gr_vector_int &ninput_items_required)
    {
      unsigned ninputs = ninput_items_required.size();
      for(unsigned i=0; i < ninputs; i++) {
        ninput_items_required[i] = (int)ceil((noutput_items * d_mu_inc) + d_resamp->ntaps());
      }
    }

    int
    fractional_resampler_cc_impl::general_work(int noutput_items,
                                               gr_vector_int &ninput_items,
                                               gr_vector_const_void_star &input_items,
                                               gr_vector_void_star &output_items)
    {
      const gr_complex *in = (const gr_complex*)input_items[0];
      gr_complex *out = (gr_complex*)output_items[0];

      int ii = 0; // input index
      int oo = 0; // output index

      if(ninput_items.size() == 1) {
        while(oo < noutput_items) {
          if (d_update_mu)
          {
            fprintf(stderr, "Updating mu: %.25Lf\n", d_mu_update);
            d_mu = d_mu_update;
            d_update_mu = false;
          }

          out[oo++] = d_resamp->interpolate(&in[ii], d_mu);
          //out[oo++] = in[ii];

          if (d_update)
          {
            fprintf(stderr, "Updating ratio: %.25Lf\n", d_mu_inc_update);
            d_mu_inc = d_mu_inc_update;
            set_relative_rate(1.0 / d_mu_inc);
            d_update = false;
          }

          long double s = d_mu + d_mu_inc;
          if (d_update_mu_adj)
          {
            s += d_mu_adj;
            fprintf(stderr, "Adjusting: %.25Lf (now: %.25Lf)\n", d_mu_adj, s);
            d_update_mu_adj = false;
          }
          long double f = floor(s);
          int incr = (int)f;
          d_mu = s - f;
          ii += incr;
        }

        consume_each(ii);
        return noutput_items;
      }

      else {
        const float *rr = (const float*)input_items[1];
        while(oo < noutput_items) {
          out[oo++] = d_resamp->interpolate(&in[ii], d_mu);
          d_mu_inc = rr[ii];

          long double s = d_mu + d_mu_inc;
          long double f = floor(s);
          int incr = (int)f;
          d_mu = s - f;
          ii += incr;
        }

        set_relative_rate(1.0 / d_mu_inc);
        consume_each(ii);
        return noutput_items;
      }
    }

    long double
    fractional_resampler_cc_impl::mu() const
    {
      return d_mu;
    }

    long double
    fractional_resampler_cc_impl::resamp_ratio() const
    {
      return d_mu_inc;
    }

    void
    fractional_resampler_cc_impl::set_mu(long double mu)
    {
      //d_mu = mu;
      d_update_mu = true;
      d_mu_update = mu;
    }

    void
    fractional_resampler_cc_impl::set_resamp_ratio(long double resamp_ratio)
    {
      //d_mu_inc = resamp_ratio;
      d_mu_inc_update = resamp_ratio;
      d_update = true;
    }

    void fractional_resampler_cc_impl::set_resamp_ratio(unsigned long long resamp_ratio_num, unsigned long long resamp_ratio_denom)
    {
      if (resamp_ratio_denom != 0)
      {
        d_mu_inc_update = (long double)resamp_ratio_num / (long double)resamp_ratio_denom;
        d_update = true;
      }
    }

  } /* namespace baz */
} /* namespace gr */
