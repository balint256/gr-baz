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

#include <gnuradio/thread/thread.h>
#include "baz_file_source.h"
#include <gnuradio/io_signature.h>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdexcept>
#include <stdio.h>

#include <vector>
#include <boost/algorithm/string.hpp>
#include <fstream>

// win32 (mingw/msvc) specific
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef O_BINARY
#define OUR_O_BINARY O_BINARY
#else
#define OUR_O_BINARY 0
#endif
// should be handled via configure
#ifdef O_LARGEFILE
#define OUR_O_LARGEFILE O_LARGEFILE
#else
#define OUR_O_LARGEFILE 0
#endif

static const pmt::pmt_t RX_TIME_KEY = pmt::string_to_symbol("rx_time");
static const pmt::pmt_t RX_RATE_KEY = pmt::string_to_symbol("rx_rate");
static const pmt::pmt_t RX_LENGTH_KEY = pmt::string_to_symbol("rx_length");

namespace gr {
  namespace baz {

    class BLOCKS_API file_source_impl : public file_source
    {
    private:
      size_t d_itemsize;
      FILE *d_fp;
      FILE *d_new_fp;
      bool d_repeat, d_repeat_new;
      volatile bool d_updated;
      boost::recursive_mutex fp_mutex;
      typedef boost::unique_lock<boost::recursive_mutex> scoped_lock;

      typedef std::pair<uint64_t,uint64_t> TimingPair;
      std::vector<TimingPair> d_timing_info, d_timing_info_new;
      double d_rate, d_rate_new;
      uint64_t d_offset, d_offset_new; // Samples
      uint64_t d_length, d_length_new; // Samples
      uint64_t d_file_length, d_file_length_new; // Samples
      uint64_t d_pad_count, d_samples;
      volatile bool d_seeked;
      bool d_pad, d_pad_new;
      int d_timing_idx;

      bool do_update();
      bool calculate_offset(uint64_t offset, uint64_t& file_offset, uint64_t& ticks, uint64_t& samples_left, uint64_t& pad_left, int& timing_idx);
      void tag(int offset, uint64_t ticks = -1);

    public:
      file_source_impl(size_t itemsize, const char *filename, bool repeat = false, long offset = 0, const char *timing_filename = NULL, bool pad = false, double rate = 0.0);
      ~file_source_impl();

      bool seek(long seek_point, int whence);
      bool seek(long seek_point) { return seek(seek_point, SEEK_SET); }
      void open(const char *filename, bool repeat = false, long offset = 0, const char *timing_filename = NULL, bool pad = false, double rate = 0.0);
      void close();
      size_t offset();
      size_t file_offset();
      double time(bool relative = false, bool raw = false);

      int work(int noutput_items,
         gr_vector_const_void_star &input_items,
         gr_vector_void_star &output_items);
    };

    file_source::sptr file_source::make(size_t itemsize, const char *filename, bool repeat/* = false*/, long offset/* = 0*/, const char *timing_filename/* = NULL*/, bool pad/* = false*/, double rate/* = 0.0*/)
    {
      return gnuradio::get_initial_sptr(new file_source_impl(itemsize, filename, repeat, offset, timing_filename, pad, rate));
    }

    file_source_impl::file_source_impl(size_t itemsize, const char *filename, bool repeat/* = false*/, long offset/* = 0*/, const char *timing_filename/* = NULL*/, bool pad/* = false*/, double rate/* = 0.0*/)
      : sync_block("file_source",
          io_signature::make(0, 0, 0),
          io_signature::make(1, 1, itemsize)),
      d_itemsize(itemsize), d_fp(0), d_new_fp(0),
      d_updated(false),
      d_rate(1.0),
      d_seeked(false),
      d_pad(pad)
    {
      fprintf(stderr, "[%s<%ld>] item size: %lu, file: %s, repeat: %s, offset: %ld, timing file: %s, pad: %s, force rate: %f\n", name().c_str(), unique_id(), itemsize, filename, (repeat ? "yes" : "no"), offset, timing_filename, (pad ? "yes" : "no"), rate);

      open(filename, repeat, offset, timing_filename, pad, rate);

      do_update();
    }

    file_source_impl::~file_source_impl()
    {
      if(d_fp)
        fclose ((FILE*)d_fp);
      if(d_new_fp)
        fclose ((FILE*)d_new_fp);
    }

    bool file_source_impl::calculate_offset(uint64_t offset, uint64_t& file_offset, uint64_t& ticks, uint64_t& samples_left, uint64_t& pad_left, int& timing_idx)
    {
      if (offset > d_length)
      {
        fprintf(stderr, "[%s<%ld>] Tried to seek off end: %lld (length: %lld)\n", name().c_str(), unique_id(), offset, d_length);
        return false;
      }
/*
      if (d_timing_info.empty())
      {
        file_offset = offset;
        ticks = (uint64_t)((double)offset * d_rate);
        samples_left = d_length - offset;
        pad_left = 0;
        timing_idx = -1;
        return true;
      }
*/
      uint64_t first_time = d_timing_info[0].first;

      if (d_timing_info.size() == 1)
      {
        file_offset = offset;
        ticks = first_time + offset;
        samples_left = d_length - offset;
        pad_left = 0;
        timing_idx = 0;

        return true;
      }

      size_t i = 1;
      for (; i <= d_timing_info.size(); ++i)
      {
        uint64_t samples = -1;
        if (i < d_timing_info.size())
          samples = d_timing_info[i].first - first_time;

        if (offset < samples)
        {
          uint64_t part = (offset + first_time) - d_timing_info[i-1].first;
          uint64_t file_part;
          uint64_t section;
          if (i < d_timing_info.size())
          {
            file_part = d_timing_info[i].second - d_timing_info[i-1].second;
            section = d_timing_info[i].first - d_timing_info[i-1].first;
          }
          else
          {
            file_part = d_length - d_timing_info[i].second;
            section = file_part;
          }

          if (part <= file_part)
          {
            file_offset = d_timing_info[i-1].second + part;
            ticks = first_time + offset;
            samples_left = file_part - part;
            pad_left = section - file_part;
            timing_idx = i - 1;
          }
          else
          {
            if (d_pad == false)
            {
              offset = samples; // Round up
              continue;
            }

            file_offset = d_timing_info[i].second;
            ticks = first_time + offset;
            samples_left = 0;
            pad_left = section - part;
            timing_idx = i - 1;
          }

          return true;
        }
      }

      file_offset = d_file_length;
      ticks = first_time + d_length;
      samples_left = 0;
      pad_left = 0;
      timing_idx = d_timing_info.size() - 1;

      fprintf(stderr, "[%s<%ld>] Clamping to end: %lld (length: %lld)\n", name().c_str(), unique_id(), offset, d_length);

      return true;
    }

    bool
    file_source_impl::seek(long seek_point, int whence) // 'seek_point' in items
    {
      scoped_lock lock(fp_mutex);

      uint64_t offset = 0;

      if (whence == SEEK_SET)
        offset = seek_point;
      else if (whence == SEEK_CUR)
        offset = d_offset + seek_point;
      else if (whence == SEEK_END)
        offset = d_length - seek_point;
      else
        return false;

      uint64_t file_offset;
      uint64_t ticks;
      uint64_t samples_left;
      uint64_t pad_left;
      int timing_idx;

      if (calculate_offset(offset, file_offset, ticks, samples_left, pad_left, timing_idx) == false)
        return false;

      fprintf(stderr, "[%s<%ld>] Seeking to offset: %llu (seek point: %ld, mode: %d, file offset: %llu)\n", name().c_str(), unique_id(), offset, seek_point, whence, file_offset);

      int res = fseek((FILE*)d_fp, (file_offset * d_itemsize), SEEK_SET);
      if (res != 0)
        return false;

      fprintf(stderr, "[%s<%ld>] Seeked to offset: %llu (samples left: %llu, pad count: %llu, timing index: %d)\n", name().c_str(), unique_id(), offset, samples_left, pad_left, timing_idx);

      d_samples = samples_left;
      d_pad_count = pad_left;
      d_seeked = true;
      d_timing_idx = timing_idx;
      d_offset = offset;

      return true;
    }

    size_t file_source_impl::offset()
    {
      scoped_lock lock(fp_mutex);

      return d_offset;
    }

    size_t file_source_impl::file_offset()
    {
      scoped_lock lock(fp_mutex);

      if (d_fp == NULL)
        return 0;

      return ftell(d_fp);
    }

    double file_source_impl::time(bool relative/* = false*/, bool raw/* = false*/)
    {
      scoped_lock lock(fp_mutex);

      // fprintf(stderr, "[%s<%ld>] Getting time: file offset: %d, absolute offset: %llu\n", name().c_str(), unique_id(), (file_offset() / d_itemsize), d_offset);

      if (relative)
      {
        if (raw)
        {
          return (double)(file_offset() / d_itemsize) / d_rate;
        }
        else
        {
          return (double)d_offset / d_rate;
        }
      }
      else
      {
        uint64_t first_time = d_timing_info[0].first;

        if (raw)
        {
          size_t offset = file_offset() / d_itemsize;
          return (double)(first_time + offset) / d_rate;
        }
        else
        {
          return (double)(first_time + d_offset) / d_rate;
        }
      }

      return 0.0; // Shouldn't ever get here
    }

    void
    file_source_impl::open(const char *filename, bool repeat/* = false*/, long offset/* = 0*/, const char *timing_filename/* = NULL*/, bool pad/* = false*/, double rate/* = 0.0*/)
    {
      if ((filename == NULL) || (filename[0] == '\0'))
        return;

      // obtain exclusive access for duration of this function
      scoped_lock lock(fp_mutex);

      int fd;

      // we use "open" to use to the O_LARGEFILE flag
      if((fd = ::open(filename, O_RDONLY | OUR_O_LARGEFILE | OUR_O_BINARY)) < 0) {
        perror(filename);
        throw std::runtime_error("can't open file");
      }

      if(d_new_fp) {
        fclose(d_new_fp);
        d_new_fp = 0;
      }

      if((d_new_fp = fdopen (fd, "rb")) == NULL) {
        perror(filename);
        ::close(fd);  // don't leak file descriptor if fdopen fails
        throw std::runtime_error("can't open file");
      }

      d_pad_new = pad;
      d_repeat_new = repeat;

      fseek(d_new_fp, 0, SEEK_END);
      d_file_length_new = ftell(d_new_fp) / d_itemsize;
      d_length_new = d_file_length_new;

      d_samples = d_length_new;
      d_pad_count = 0;

      fseek(d_new_fp, 0, SEEK_SET);

      fprintf(stderr, "[%s<%ld>] Opened: %s (%llu samples)\n", name().c_str(), unique_id(), filename, d_file_length_new);

      d_offset_new = offset;

      d_timing_info_new.clear();
      d_rate_new = ((rate <= 0.0) ? 1.0 : rate);

      // FIXME: Handle when file doesn't match timing info (e.g. from a loop)

      if ((timing_filename != NULL) && (timing_filename[0] != '\0'))
      {
        std::ifstream timing_file(timing_filename);

        if (timing_file.is_open())
        {
          std::string str;

          uint64_t last_sample_count = -1;

          while (true)
          {
            timing_file >> str;
            if (timing_file.good() == false)
              break;

            boost::trim(str);
            
            if (str.empty())
              continue;

            if (str[0] == '#')
              continue;

            if (toupper(str[0]) == 'R')
            {
              d_rate_new = atof(str.substr(1).c_str());
              if (d_rate_new <= 0.0)
              {
                fprintf(stderr, "[%s<%ld>] Invalid rate line: %s\n", name().c_str(), unique_id(), str.c_str());
                throw std::runtime_error("invalid rate in timing file");

                d_rate_new = 1.0;
              }

              continue;
            }

            int idx = str.find(',');
            if (idx == -1)
              continue;

            uint64_t ticks = atol(str.substr(0, idx).c_str());
            uint64_t sample_count = atol(str.substr(idx + 1).c_str());

            d_timing_info_new.push_back(std::make_pair(ticks, sample_count));

            last_sample_count = sample_count;
          }

          if (d_timing_info_new.size() >= 2)
          {
            uint64_t last_count = d_length_new - last_sample_count;

            uint64_t tick_diff = d_timing_info_new[d_timing_info_new.size() - 1].first - d_timing_info_new[0].first;
            
            d_length_new = tick_diff + last_count;

            ///////////////////////////

            uint64_t total_samples = d_timing_info_new[1].first - d_timing_info_new[0].first;

            d_samples = d_timing_info_new[1].second - d_timing_info_new[0].second;
            d_pad_count = total_samples - d_samples;
          }

          fprintf(stderr, "[%s<%ld>] Read %lu timing points from: %s (rate: %f, length: %llu)\n", name().c_str(), unique_id(), d_timing_info_new.size(), timing_filename, d_rate_new, d_length_new);
        }
        else
        {
          fprintf(stderr, "[%s<%ld>] Failed to open: %s\n", name().c_str(), unique_id(), timing_filename);
          throw std::runtime_error("cannot open timing file");
        }
      }

      if (d_timing_info_new.empty())
      {
        d_timing_info_new.push_back(std::make_pair((uint64_t)0, (uint64_t)0));
      }

      d_updated = true;
    }

    void
    file_source_impl::close()
    {
      // obtain exclusive access for duration of this function
      scoped_lock lock(fp_mutex);

      if(d_new_fp != NULL) {
        fclose(d_new_fp);
        d_new_fp = NULL;
      }

      d_updated = true;
    }

    bool
    file_source_impl::do_update()
    {
      if (d_updated) {
        scoped_lock lock(fp_mutex); // hold while in scope

        if(d_fp)
          fclose(d_fp);

        d_fp = d_new_fp;    // install new file pointer
        d_new_fp = 0;

        d_pad = d_pad_new;
        d_repeat = d_repeat_new;

        d_length = d_length_new;
        d_file_length = d_file_length_new;

        d_timing_info = d_timing_info_new;
        d_rate = d_rate_new;

        d_offset = 0;

        d_samples = d_length;
        d_pad_count = 0;

        if (seek(d_offset_new)) // After new FP installation
        {
          // d_offset = d_offset_new; // Now done in 'seek'
        }
        else
        {
          throw std::runtime_error("Failed to seek during update");

          fprintf(stderr, "[%s<%ld>] Seek failed during update (leaving offset at 0)\n", name().c_str(), unique_id());

          if (seek(0) == false) // Update variables
            throw std::runtime_error("Failed to seek to beginning");
        }

        // d_seeked = true; // Set inside 'seek'
        d_updated = false;

        fprintf(stderr, "[%s<%ld>] Updated (offset: %llu)\n", name().c_str(), unique_id(), d_offset);

        return true;
      }

      return false;
    }

    void file_source_impl::tag(int offset, uint64_t ticks/* = -1*/)
    {
      fprintf(stderr, "[%s<%ld>] Tagging at sample offset %d (count: %llu), ticks: %lld\n", name().c_str(), unique_id(), offset, (nitems_written(0) + offset), (int64_t)ticks);

      add_item_tag(0, nitems_written(0) + offset, RX_RATE_KEY, pmt::from_double(d_rate));
      add_item_tag(0, nitems_written(0) + offset, RX_LENGTH_KEY, pmt::from_uint64(d_length));

      if (ticks == -1)
        ticks = d_timing_info[0].first + d_offset;

      if (d_rate == 1.0)
      {
        fprintf(stderr, "[%s<%ld>] Time tag: %llu (from offset: %llu)\n", name().c_str(), unique_id(), ticks, d_offset);

        add_item_tag(0, nitems_written(0) + offset, RX_TIME_KEY, pmt::make_tuple(pmt::from_uint64(ticks), pmt::from_double(0.0)));
      }
      else
      {
        uint64_t whole_seconds = (uint64_t)((double)ticks / d_rate);
        double frac_seconds = (double)(ticks - (uint64_t)((double)whole_seconds * d_rate)) / d_rate;

        fprintf(stderr, "[%s<%ld>] Time tag: %llu, %f (from offset: %llu, ticks: %llu)\n", name().c_str(), unique_id(), whole_seconds, frac_seconds, d_offset, ticks);

        add_item_tag(0, nitems_written(0) + offset, RX_TIME_KEY, pmt::make_tuple(pmt::from_uint64(whole_seconds), pmt::from_double(frac_seconds)));
      }
    }

    int
    file_source_impl::work(int noutput_items,
         gr_vector_const_void_star &input_items,
         gr_vector_void_star &output_items)
    {
      char *o = (char*)output_items[0];
      int size = noutput_items;

      bool updated = do_update();

      if(d_fp == NULL)
        throw std::runtime_error("work with file not open");

      scoped_lock lock(fp_mutex); // hold for the rest of this function

      uint64_t tag_time = -1;

      if (((d_pad) && (d_samples == 0) && (d_pad_count == 0)) ||
          ((d_pad == false) && (d_samples == 0)))
      {
        ++d_timing_idx;
/*
        if (d_timing_idx == d_timing_info.size())
        {
          throw std::runtime_error("invalid timing index");
        }
*/
        if (d_timing_idx < d_timing_info.size())
        {
          TimingPair& tp = d_timing_info[d_timing_idx];

          uint64_t offset = tp.first - d_timing_info[0].first;

          if ((d_offset + d_pad_count) != offset)
          {
            fprintf(stderr, "[%s<%ld>] Offset mismatch: %llu != %llu (timing index: %d, ticks: %llu, file offset: %llu)\n", name().c_str(), unique_id(), d_offset, offset, d_timing_idx, tp.first, tp.second);
          }

          if (d_timing_idx == (d_timing_info.size() - 1))
          {
            d_samples = d_length - tp.second;
            d_pad_count = 0;
          }
          else
          {
            d_samples = d_timing_info[d_timing_idx + 1].second - tp.second;
            d_pad_count = (d_timing_info[d_timing_idx + 1].first - tp.first) - d_samples;
          }

          d_offset = offset;

          tag_time = d_timing_info[d_timing_idx].first;

          // updated = true;
          d_seeked = true;
        }
        else
        {
          // FIXME: Verify we're at EOF
        }
      }

      if ((d_pad) && (d_samples == 0) && (d_pad_count > 0))
      {
        if (d_seeked)
        {
          tag(0, tag_time);
          d_seeked = false;
        }

        uint64_t n = std::min((uint64_t)noutput_items, d_pad_count);
        memset(o, 0x00, (d_itemsize * n));
        o += (d_itemsize * n);
        d_pad_count -= n;
        d_offset += n;

        return n;
      }

      /////////////////////////////////

      if (d_timing_idx < d_timing_info.size())
      {
        uint64_t _size = (uint64_t)size;
        if (d_samples < _size)
          size = (int)d_samples;
      }
      else
      {
        // Allow EOF/repeat
      }

      int ii = 0;
      while (size)
      {
        int i = fread(o, d_itemsize, size, (FILE*)d_fp);

        if (i > 0)
        {
          if (d_seeked)
          {
            tag(ii, tag_time);  // 'ii' should always be '0'
            d_seeked = false;
          }

          ii += i;
          size -= i;
          if (d_samples >= i)
            d_samples -= i;
          else
          {
            fprintf(stderr, "[%s<%ld>] Residual read: %d\n", name().c_str(), unique_id(), i);
          }
          d_offset += i;
          o += i * d_itemsize;
        }

        if (size == 0)   // done
          break;

        /////////////////////

        if (i > 0)     // short read, try again
        {
          fprintf(stderr, "[%s<%ld>] Short read: %d of %d\n", name().c_str(), unique_id(), i, (size + i));
          continue;
        }

        // We got a zero from fread.  This is either EOF or error.  In
        // any event, if we're in repeat mode, seek back to the beginning
        // of the file and try again, else break
        if (!d_repeat)
        {
          fprintf(stderr, "[%s<%ld>] EOF\n", name().c_str(), unique_id());
          break;
        }

        // if (fseek ((FILE *) d_fp, 0, SEEK_SET) == -1) {
        //   fprintf(stderr, "[%s] fseek failed\n", __FILE__);
        //   exit(-1);
        // }

        if (seek(0) == false) // FIXME: Or to original offset?
          return -1;

        fprintf(stderr, "[%s<%ld>] Repeating\n", name().c_str(), unique_id());
      }

      if (size > 0) {          // EOF or error
        if (size == noutput_items)       // we didn't read anything; say we're done
          return -1;

        return (noutput_items - size);  // else return partial result
      }

      return noutput_items;
    }

  } /* namespace baz */
} /* namespace gr */
