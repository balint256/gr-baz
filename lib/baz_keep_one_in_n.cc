/* -*- c++ -*- */
/*
 * Copyright 2012 Free Software Foundation, Inc.
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

#include "baz_keep_one_in_n.h"
#include <gnuradio/io_signature.h>

static const pmt::pmt_t SOB_KEY = pmt::string_to_symbol("tx_sob");
static const pmt::pmt_t EOB_KEY = pmt::string_to_symbol("tx_eob");
static const pmt::pmt_t OFFSET_KEY = pmt::string_to_symbol("offset");

namespace gr {
  namespace baz {

    class BAZ_API keep_one_in_n_impl : public keep_one_in_n
    {
      int   d_n;
      int   d_count;
      float d_decim_rate;
      std::vector<gr::tag_t> d_tag_buffer;
      bool d_in_burst;
      uint64_t d_next_eob;
      bool d_verbose;

    public:
      keep_one_in_n_impl(size_t itemsize,int n, bool verbose);

      void forecast(int noutput_items, gr_vector_int &ninput_items_required);

      int general_work(int noutput_items,
           gr_vector_int &ninput_items,
           gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);

      void set_n(int n);
    };

    keep_one_in_n::sptr keep_one_in_n::make(size_t itemsize, int n, bool verbose)
    {
      return gnuradio::get_initial_sptr(new keep_one_in_n_impl(itemsize, n, verbose));
    }

    keep_one_in_n_impl::keep_one_in_n_impl(size_t itemsize, int n, bool verbose/* = false*/)
      : block("keep_one_in_n",
      io_signature::make (1, 1, itemsize),
      io_signature::make (1, 1, itemsize)),
      d_count(n),
      d_in_burst(false),
      d_next_eob(-1),
      d_verbose(verbose)
    {
      // To avoid bad behavior with using set_relative_rate in this block with
      // VERY large values of n, we will keep track of things ourselves. Using
      // this to turn off automatic tag propagation, which will be handled
      // locally in general_work().
      set_tag_propagation_policy(TPP_DONT);

      set_n(n);
    }

    void
    keep_one_in_n_impl::set_n(int n)
    {
      if (n < 1)
        n = 1;

      d_n = n;
      d_count = n;

      // keep our internal understanding of the relative rate of this block
      // don't set the relative rate, though, and we will handle our own
      // tag propagation.
      d_decim_rate = 1.0/(float)d_n;

      if (d_verbose) fprintf(stderr, "[%s<%ld>] Relative rate: %f (N: %d)\n", name().c_str(), unique_id(), d_decim_rate, d_n);
      // [One or the other] See forecast
      // set_relative_rate(d_decim_rate);
      // set_output_multiple(n);
    }

    void keep_one_in_n_impl::forecast(int noutput_items, gr_vector_int &ninput_items_required)
    {
      for (size_t i = 0; i < ninput_items_required.size(); ++i)
      {
        ninput_items_required[i] = (noutput_items * d_n);
      }
    }

    int
    keep_one_in_n_impl::general_work(int noutput_items,
             gr_vector_int &ninput_items,
             gr_vector_const_void_star &input_items,
             gr_vector_void_star &output_items)
    {
      const char *in = (const char *) input_items[0];
      char *out = (char *) output_items[0];

      size_t item_size = input_signature()->sizeof_stream_item(0);
      int    ni = 0;
      int    no = 0;

      // fprintf(stderr, "[%s<%ld>] Work: In: %d, Out: %d\n", name().c_str(), unique_id(), ninput_items[0], noutput_items);

      if (ninput_items[0] < d_n)
      {
        fprintf(stderr, "[%s<%ld>] Not enough input!\n", name().c_str(), unique_id());
        return -1;
      }

      while ((ni <= (ninput_items[0] - d_n)) && (no < noutput_items)) {  // Have enough input to be able to look ahead
        /*if (d_count == d_n) {
          std::vector<tag_t> tags;
          // get_tags_in_range(tags, 0, (nitems_read(0) + (ni + 1)), std::min((nitems_read(0) + (ni + 1) + (d_n - 1)), (nitems_read(0) + ninput_items[0])), SOB_KEY); // [,)
          get_tags_in_range(tags, 0, (nitems_read(0) + ni), std::min((nitems_read(0) + ni + d_n), (nitems_read(0) + ninput_items[0])), SOB_KEY); // [,)
          std::sort(tags.begin(), tags.end(), gr::tag_t::offset_compare);
          if (tags.empty() == false)
          {
            tag_t& t0 = tags[0];
            d_count = (t0.offset - (nitems_read(0) + ni)) + 1; // + 1 to counteract '--' below
            fprintf(stderr, "[%lld] Found SOB in next block. Setting count to: %d\n", (nitems_read(0) + ni), (d_count - 1));
          }
        }*/

        bool new_burst = false;

        {
          std::vector<tag_t> tags;
          get_tags_in_range(tags, 0, (nitems_read(0) + ni), (nitems_read(0) + ni + 1), SOB_KEY);
          if (tags.empty() == false)
          {
            tag_t& t0 = tags[0];
            d_count = (t0.offset - (nitems_read(0) + ni));
            if (d_verbose) fprintf(stderr, "[%s<%ld>] Found SOB at %lld. Setting count to: %d\n", name().c_str(), unique_id(), (nitems_read(0) + ni), d_count);

            // Buffered tags after EOB will be applied to upcoming output sample

            // FIXME: Add tag for SOB offset

            d_count += 1; // + 1 to counteract '--' below
            d_in_burst = true;
            new_burst = true;
          }
        }

        if ((d_next_eob == -1) || ((nitems_read(0) + ni) > d_next_eob))
        {
          if ((d_in_burst) && (d_next_eob != -1))
          {
            fprintf(stderr, "[%s<%ld>] In burst while next EOB found! [2]\n", name().c_str(), unique_id());
            return -1;
            d_in_burst = false;
          }

          std::vector<tag_t> tags;
          get_tags_in_range(tags, 0, (nitems_read(0) + ni), (nitems_read(0) + ni + 1));
          BOOST_FOREACH(gr::tag_t& tag, tags)
          {
            if (pmt::eq(tag.key, EOB_KEY))
            {
              fprintf(stderr, "[%s<%ld>] Found EOB during tag buffering!\n", name().c_str(), unique_id());
              continue;
            }

            d_tag_buffer.push_back(tag);
          }
        }

        {
          std::vector<tag_t> tags;
          get_tags_in_range(tags, 0, (nitems_read(0) + ni), (nitems_read(0) + ni + 1), EOB_KEY);
          if (tags.empty() == false)
          {
            tag_t& t0 = tags[0];

            if (d_next_eob == t0.offset)
            {
              d_next_eob = -1;

              if (d_verbose) fprintf(stderr, "[%s<%ld>] Found next EOB at %lld\n", name().c_str(), unique_id(), t0.offset);

              if (d_in_burst)
              {
                fprintf(stderr, "[%s<%ld>] In burst while next EOB found! [1]\n", name().c_str(), unique_id());
                return -1;
                d_in_burst = false;
              }
            }
          }
        }

        d_count--;

        if (d_count <= 0) {
          memcpy (out, in, item_size);    // copy 1 item
          out += item_size;

          BOOST_FOREACH(gr::tag_t tag, d_tag_buffer) // Copy
          {
            uint64_t prev_offset = tag.offset;
            tag.offset = nitems_written(0) + no;
            if (d_verbose) fprintf(stderr, "[%s<%ld>] (read: %lld) (buffered) %s: %lld -> %lld\n", name().c_str(), unique_id(), nitems_read(0), pmt::write_string(tag.key).c_str(), prev_offset, tag.offset);
            add_item_tag(0, tag);
          }

          d_tag_buffer.clear();

          ///////////////////

          if (new_burst)
          {
            std::vector<tag_t> tags;
            get_tags_in_range(tags, 0, (nitems_read(0) + ni), (nitems_read(0) + ni + 1), OFFSET_KEY);

            if (tags.empty())
            {
              gr::tag_t tag;
              tag.offset = nitems_written(0) + no;
              tag.key = OFFSET_KEY;
              tag.value = pmt::from_long(nitems_read(0) + ni);
              add_item_tag(0, tag);
            }
          }

          // FIXME: SOB offset

          ///////////////////

          if (d_in_burst)
          {
            if ((ni + d_n) > ninput_items[0])
            {
              fprintf(stderr, "[%s<%ld>] Out of buffer!\n", name().c_str(), unique_id());
              break;
            }

            std::vector<tag_t> tags;
            get_tags_in_range(tags, 0, (nitems_read(0) + ni), (nitems_read(0) + ni + d_n), EOB_KEY); // [,)
            if (tags.empty() == false)
            {
              std::sort(tags.begin(), tags.end(), gr::tag_t::offset_compare);
              tag_t& t0 = tags[0];

              if (d_verbose) fprintf(stderr, "[%s<%ld>] Found future EOB at %lld (+%lld)\n", name().c_str(), unique_id(), t0.offset, (t0.offset - (nitems_read(0) + ni)));

              bool end_burst = true;

              {
                std::vector<tag_t> tags;
                get_tags_in_range(tags, 0, (nitems_read(0) + ni), (t0.offset + 1), SOB_KEY); // [,)
                if (tags.empty() == false)
                {
                  std::sort(tags.begin(), tags.end(), gr::tag_t::offset_compare);
                  tag_t& t1 = tags[0];
                  if (t1.offset < t0.offset)
                  {
                    fprintf(stderr, "[%s<%ld>] SOB (%lld) before EOB (%lld) while in burst!\n", name().c_str(), unique_id(), t1.offset, t0.offset);
                    end_burst = false;
                    // FIXME: Figure out how to deal with tags
                  }
                }
              }

              if (end_burst)
              {
                d_next_eob = t0.offset;
                d_in_burst = false;

                // FIXME: Modify d_count?

                {
                  std::vector<tag_t> tags;
                  get_tags_in_range(tags, 0, (nitems_read(0) + ni), (d_next_eob + 1)); // [,)
                  BOOST_FOREACH(gr::tag_t tag, tags) // Copy
                  {
                    uint64_t prev_offset = tag.offset;
                    tag.offset = nitems_written(0) + no;
                    if (d_verbose) fprintf(stderr, "[%s<%ld>] (read: %lld) (before EOB) %s: %lld -> %lld\n", name().c_str(), unique_id(), nitems_read(0), pmt::write_string(tag.key).c_str(), prev_offset, tag.offset);
                    add_item_tag(0, tag);
                  }
                }
              }
            }
          }

          ///////////////////

          no++;
          d_count = d_n;
        }

        in += item_size;
        ni++;
      }
/*
      // Because we have set TPP_DONT, we have to propagate the tags here manually.
      // Adjustment of the tag sample value is done using the float d_decim_rate.
      std::vector<tag_t> tags;
      std::vector<tag_t>::iterator t;
      get_tags_in_range(tags, 0, nitems_read(0), nitems_read(0)+ni);
      for(t = tags.begin(); t != tags.end(); t++) {
        tag_t new_tag = *t;
        uint64_t o = new_tag.offset;
        new_tag.offset *= d_decim_rate;
        new_tag.offset = std::max(nitems_written(0), new_tag.offset);
        fprintf(stderr, "[%lld] %s: %lld -> %lld\n", nitems_read(0), pmt::write_string(new_tag.key).c_str(), o, new_tag.offset);
        add_item_tag(0, new_tag);
      }
*/
      consume_each (ni);

      return no;
    }

  } /* namespace blocks */
} /* namespace gr */
