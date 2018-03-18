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

#include "baz_dpll_bb.h"
#include <gnuradio/io_signature.h>

#include <cstdio>
#include "stdio.h"

namespace gr {
  namespace baz {

    class dpll_bb_impl : public dpll_bb
    {
    private:
      unsigned char d_restart;
      /*float*/double d_pulse_phase, d_pulse_frequency;
      /*float*/double d_gain, d_decision_threshold;
      /*float*/double d_period;
      uint64_t d_count;
      bool d_verbose;
      /*float*/double d_relative_limit;
      /*float*/double d_ignore_limit;
      pmt::pmt_t d_length_tag;
      /*float*/double d_original_period;
      bool d_unlocked;
      int64_t d_last_pulse_idx;

    public:
      dpll_bb_impl(float period, float gain, float relative_limit = 1.0, float ignore_limit = 1.0, const std::string length_tag = "", bool verbose = false, bool unlocked = false);
      ~dpll_bb_impl();

      void set_gain(float gain) { d_gain = gain; }
      void set_decision_threshold(float thresh) { d_decision_threshold = thresh; }

      float gain() const { return d_gain; }
      float freq() const { return d_pulse_frequency; }
      float phase() const { return d_pulse_phase; }
      float decision_threshold() const { return d_decision_threshold; }

      int work(int noutput_items,
           gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);

      //void forecast(int noutput_items, gr_vector_int &ninput_items_required);
      /*int general_work(int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items);*/
    };

    dpll_bb::sptr
    dpll_bb::make(float period, float gain, float relative_limit, float ignore_limit, const std::string length_tag, bool verbose, bool unlocked)
    {
      return gnuradio::get_initial_sptr(new dpll_bb_impl(period, gain, relative_limit, ignore_limit, length_tag, verbose, unlocked));
    }

    dpll_bb_impl::dpll_bb_impl(float period, float gain, float relative_limit, float ignore_limit, const std::string length_tag, bool verbose, bool unlocked)
      : sync_block("dpll_bb",
              io_signature::make(1, 2, sizeof(char)),
              io_signature::make2(0, 2, sizeof(char), sizeof(float))),
    d_restart(0), d_pulse_phase(0),
    d_period(period), d_count(0),
    d_relative_limit(relative_limit), d_ignore_limit(ignore_limit),
    d_verbose(verbose), d_original_period(period), d_unlocked(unlocked),
    d_last_pulse_idx(-1)
    {
      if (length_tag.size() > 0)
        d_length_tag = pmt::mp(length_tag);

      fprintf(stderr, "[%s<%ld>] period: %f, gain: %f, relative limit: %f, ignore limit: %f, length tag: \'%s\', verbose: %s, unlocked: %s\n", name().c_str(), unique_id(), period, gain, relative_limit, ignore_limit, length_tag.c_str(), (verbose ? "yes" : "no"), (unlocked ? "yes" : "no"));

      if (unlocked == false)
      {
        // FIXME: Change to set_output_multiple
        set_history(1 + int(period / 2.0));
        if (d_verbose) fprintf(stderr, "[%s<%ld>] History: %d\n", name().c_str(), unique_id(), history());
        //set_output_multiple(int(ceil(period / 2.0)));
        //set_min_output_buffer(int(ceil(period/* / 2.0*/)));
        if (d_verbose) fprintf(stderr, "[%s<%ld>] Min output buffer: %ld\n", name().c_str(), unique_id(), min_output_buffer(0));
        //set_min_noutput_items(int(ceil(period / 2.0)));
        if (d_verbose) fprintf(stderr, "[%s<%ld>] Min noutput items: %d\n", name().c_str(), unique_id(), min_noutput_items());
      }

      d_pulse_frequency = 1.0 / period;
      d_gain = gain;
      d_decision_threshold = 1.0 - 0.5*d_pulse_frequency;

      message_port_register_out(pmt::mp("out"));
#if 0
      fprintf(stderr,"frequency = %f period = %f gain = %f threshold = %f\n",
          d_pulse_frequency,
          period,
          d_gain,
          d_decision_threshold);
#endif
    }

    dpll_bb_impl::~dpll_bb_impl()
    {
    }
/*
    void dpll_bb_impl::forecast(int noutput_items, gr_vector_int &ninput_items_required)
    {
      //if (d_verbose) fprintf(stderr, "[%s<%ld>] forecast: %d\n", name().c_str(), unique_id(), noutput_items);

      for (int i = 0; i < ninput_items_required.size(); ++i)
        ninput_items_required[i] = noutput_items;
    }
*/
    // FIXME: Why didn't history work with non-sync block? Missing a setup call somewhere?
    int dpll_bb_impl::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    //int dpll_bb_impl::general_work(int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    {
      const char *iptr = (const char*)input_items[0];
      const char *reset = NULL;
      if (input_items.size() > 1)
        reset = (const char*)input_items[1];

      char *optr = NULL;
      if (output_items.size() > 0)
        optr = (char*)output_items[0];
      float *out_period = NULL;
      if (output_items.size() > 1)
        out_period = (float*)output_items[1];

      pmt::pmt_t msg_out_id = pmt::mp("out");

      int look_ahead = history()/*int(ceil(d_original_period / 2.0))*/;

      /*if (noutput_items < look_ahead)
      {
        if (d_verbose) fprintf(stderr, "[%s<%ld>] Buffer too small! %d < %d\n", name().c_str(), unique_id(), noutput_items, look_ahead);
        look_ahead = noutput_items;
      }*/

      //if (d_verbose) if (d_verbose) fprintf(stderr, "[%s<%ld>] Work on count %lld\n", name().c_str(), unique_id(), d_count);

        // FIXME: Read 'rx_time' tag and keep track of time internally
        // If future tag arrives, adjust 'd_pulse_phase'

        // FIXME: Auto-period mode (supply 0 and it'll auto-compute from first two transitions, or average/median from the first N)

        // FIXME: Unlocked mode
        // Start 'locked' and after a few pulses, transition (once period has been established), option to output pulses during this time
        // One mode would be to output pulses when they arrive as 'locked' mode, except for those that fall outside ignore/relative/(new) limit (i.e. noise supression as below) <- just another param, run in normal 'locked' mode
        // Main idea: have two PLLs: one is the existing one only to track the period of the incoming pulses, however any incoming pulse does NOT get output
        // Instead, second PLL attempts to lock to the first: 2nd needs to track period of the 1st (phase of 2nd will be out-of-sync with first, but will change nicely w.r.t itself)
        // Once period is locked, can alter phase to match input pulses? (like original DPLL block)

        for (int i = 0; i < noutput_items; i++)
        {
          if (reset != NULL)  // FIXME: Also on incoming tag
          {
            if (reset[i] != 0)
            {
              if (d_verbose) fprintf(stderr, "[%s<%ld>] Reset on count %lld\n", name().c_str(), unique_id(), d_count);

              d_count = 0;
              d_pulse_phase = 0.0;
              // Period remains as is
            }
          }

          if (optr != NULL)
            optr[i] = 0;
          if (out_period != NULL)
            out_period[i] = 0.0f;

          if (iptr[i] != 0)
          {
            /*float*/double current_period = d_pulse_phase / d_pulse_frequency;
            /*float*/double diff = current_period - d_period;
            /*float*/double ratio = diff / d_period;  // -ve: early, +ve: late

            //if (d_verbose) fprintf(stderr, "[%s<%ld>] Pulse at sample %lld\n", name().c_str(), unique_id(), nitems_read(0)+i);

            int64_t current_pulse_idx = nitems_read(0)+i;
            if (d_last_pulse_idx > -1)
            {
              int64_t diff = current_pulse_idx - d_last_pulse_idx;
              pmt::pmt_t dict = pmt::make_dict();
              dict = pmt::dict_add(dict, pmt::mp("diff"), pmt::from_long(diff));
              dict = pmt::dict_add(dict, pmt::mp("period"), pmt::from_double(d_period));
              dict = pmt::dict_add(dict, pmt::mp("current_period"), pmt::from_double(current_period));
              message_port_pub(msg_out_id, dict);
            }
            d_last_pulse_idx = current_pulse_idx;

            /////////////////////////////////////////

            if ((d_unlocked == false) || (d_count == 0))  // FIXME: Noise supression with limits
            {
              if (optr != NULL)
                optr[i] = 1;
              if (out_period != NULL)
                out_period[i] = d_period;

              if ((d_length_tag) && (pmt::eq(d_length_tag, pmt::PMT_NIL) == false))
              {
                add_item_tag(0, nitems_written(0)+i, d_length_tag, pmt::from_long((long)d_period));
              }
            }

            if (d_count > 0)
            {
              if ((d_pulse_phase <= d_decision_threshold) ||  // Early
                  (d_pulse_phase > (d_decision_threshold + d_pulse_frequency)))  // Late (cannot be late in unlocked mode)
              {
                if (fabs(ratio) < d_ignore_limit)
                {
                  if (fabs(ratio) >= d_relative_limit)
                  {
                    if (ratio >= 0) // Late
                      current_period = d_period * (1.0 + d_relative_limit);
                    else  // Early
                      current_period = d_period * (1.0 - d_relative_limit);
                  }

                  if ((d_length_tag) && (pmt::eq(d_length_tag, pmt::PMT_NIL) == false)) // FIXME: Arg
                  {
                    add_item_tag(0, nitems_written(0)+i, pmt::mp("current_period"), pmt::from_float(current_period));
                  }

                  //d_period += (diff * d_gain);
                  /*float*/double new_period = (1.0 - d_gain) * d_period + (d_gain * current_period);

                  if (d_verbose)
                  {
                    if (fabs(ratio) >= d_relative_limit)
                      fprintf(stderr, "[%s<%ld>] Clamping period adjustment on count %lld: current: %f, clamped: %f, previous: %f, new: %f (diff: %f, ratio: %f)\n", name().c_str(), unique_id(), d_count, (d_pulse_phase / d_pulse_frequency), current_period, d_period, new_period, diff, ratio);
                    else
                      fprintf(stderr, "[%s<%ld>] Adjusting period on count %lld: current: %f, previous: %f, new: %f (diff: %f, ratio: %f)\n", name().c_str(), unique_id(), d_count, current_period, d_period, new_period, diff, ratio);
                  }

                  if (d_unlocked)
                  {
                    assert(d_pulse_phase <= d_decision_threshold);  // Cannot be late in unlocked mode
                    assert(ratio < 0);  // Can only be early

                    d_pulse_phase = 1.0 - (((1.0 - d_pulse_phase) * d_period) * (1.0 / new_period));

                    assert(d_pulse_phase < 1.0);
                  }

                  d_period = new_period;
                  d_pulse_frequency = 1.0 / new_period;
                }
                else
                {
                  if (d_verbose) fprintf(stderr, "[%s<%ld>] Ignoring period adjustment on count %lld: current: %f, previous: %f (diff: %f, ratio: %f)\n", name().c_str(), unique_id(), d_count, current_period, d_period, diff, ratio);

                  /*if (d_count == 1)
                  {
                    if (d_verbose) fprintf(stderr, "[%s<%ld>] Resetting\n", name().c_str(), unique_id());

                    d_count = 0;
                  }*/
                }
              }
              else  // Coincides with incoming pulse
              {
                if (d_unlocked)
                {
                  if (optr != NULL)
                    optr[i] = 1;
                  if (out_period != NULL)
                    out_period[i] = d_period;

                  if ((d_length_tag) && (pmt::eq(d_length_tag, pmt::PMT_NIL) == false))
                  {
                    add_item_tag(0, nitems_written(0)+i, d_length_tag, pmt::from_long((long)d_period));
                  }

                  if (d_verbose) fprintf(stderr, "[%s<%ld>] Coinciding pulse on count %lld: previous: %f\n", name().c_str(), unique_id(), d_count, d_period);

                  d_count++;
                  d_pulse_phase -= 1.0;
                }
              }
            }

            if ((d_unlocked == false) || (d_count == 0))
            {
              //if (d_count == 0)
              //  if (d_verbose) fprintf(stderr, "[%s<%ld>] First incoming pulse\n", name().c_str(), unique_id());

              d_count++;
              d_pulse_phase = 0.0;  // FIXME: For unlocked mode
            }
          }
          else if (d_count == 0)  // Wait until first pulse
          {
              continue;
          }
          else if ((d_pulse_phase > d_decision_threshold) && ((d_unlocked) || (d_pulse_phase <= (d_decision_threshold + d_pulse_frequency))))
          {
            if (d_unlocked == false)
            {
              if (i > 0)
              {
                //if (d_verbose) fprintf(stderr, "[%s<%ld>] Re-adjusting buffer for history %d\n", name().c_str(), unique_id(), look_ahead);

                //consume(0 , i);
                //consume_each(i);

                return i;
              }
            }

            // Would-be output '1' is at beginning of history now

            int j = 1;

            if (d_unlocked == false)
            {
              //if (d_verbose) fprintf(stderr, "[%s<%ld>] Looking for late pulse at offset %d in history %d\n", name().c_str(), unique_id(), i, look_ahead);

              for (; j < look_ahead; ++j)
              {
                  if (iptr[j] == 1)
                  {
                    //if (d_verbose) fprintf(stderr, "[%s<%ld>] Found late pulse arriving in %d\n", name().c_str(), unique_id(), j);

                    break;
                  }
              }
            }

            if ((d_unlocked) || (j == look_ahead))    // No late pulse found
            {
                if (optr != NULL)
                  optr[i] = 1;
                if (out_period != NULL)
                  out_period[i] = d_period;

                if ((d_length_tag) && (pmt::eq(d_length_tag, pmt::PMT_NIL) == false))
                {
                  add_item_tag(0, nitems_written(0)+i, d_length_tag, pmt::from_long((long)d_period));
                }

                if (d_verbose) fprintf(stderr, "[%s<%ld>] Outputting pulse where none was detected on count %lld\n", name().c_str(), unique_id(), d_count);

                d_count++;
                d_pulse_phase -= 1.0;
            }
            else    // Late pulse coming
            {
                // Continue as normal

              //if (d_verbose) fprintf(stderr, "[%s<%ld>] Late pulse arriving in %d\n", name().c_str(), unique_id(), j);
            }
          }

          d_pulse_phase += d_pulse_frequency;
        }
/*
  for(int i = 0; i < noutput_items; i++) {
    optr[i]= 0;
    if(iptr[i] == 1) {
      if(d_restart == 0) {
        d_pulse_phase = 1;
      }
      else {
        if(d_pulse_phase > 0.5)
          d_pulse_phase += d_gain*(1.0-d_pulse_phase);
        else
          d_pulse_phase -= d_gain*d_pulse_phase;
      }
      d_restart = 3;
    }
    if(d_pulse_phase > d_decision_threshold) {
      
      if(d_restart > 0) {
        d_restart -= 1;
        optr[i] = 1;
      }
    }
    d_pulse_phase += d_pulse_frequency;
      }
*/
      //consume(0, noutput_items);
      //consume_each(noutput_items);
      return noutput_items;
    }

  } /* namespace baz */
} /* namespace gr */
