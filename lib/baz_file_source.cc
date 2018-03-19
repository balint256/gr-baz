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
#include <arpa/inet.h>

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
static const pmt::pmt_t RX_FREQ_KEY = pmt::string_to_symbol("rx_freq"); // FIXME: Implement this

#ifndef FOURCC
#define FOURCC(a,b,c,d) (((a) << 24) | ((b) << 16) | ((c) << 8) | ((d) << 0))
#endif // FOURCC

static std::string fourcc_to_string(uint32_t fourcc)
{
  char s[5];
  memset(s, 0x00, sizeof(s));
  fourcc = htonl(fourcc);
  memcpy(s, &fourcc, sizeof(s));
  return std::string(s);
}

namespace gr {
  namespace baz {
    class InputFile
    {
    public:
      typedef struct WaveFormatEx
      {
        uint16_t format;
        uint16_t channels;
        uint32_t sample_rate;
        uint32_t average_bytes_per_sec;
        uint16_t block_align;
        uint16_t bits_per_sample;
      } WAVE_FORMAT_EX;
      typedef struct SystemTime
      {
        uint16_t year, month, day_of_week, day, hour, minute, second, millisecond;
      } SYSTEM_TIME;
      typedef struct Auxi
      {
        SYSTEM_TIME time_start;
        SYSTEM_TIME time_end;
        long freq1;
        char padding[24];
        long freq2;
      } AUXI;
      enum FileType
      {
        FT_RAW,
        FT_WAVE
      };
      typedef std::shared_ptr<InputFile> sptr;
      typedef std::pair<uint64_t,uint64_t> TimingPair;
    public:
      InputFile(const std::string& path, size_t item_size_hint = 0, double sample_rate_hint = 0, double freq_hint = 0, const std::string& timing_path = std::string())
        : m_path(path)
        , m_path_timing(timing_path)
        , m_item_size_hint(item_size_hint)
        , m_sample_rate_hint(sample_rate_hint)
        , m_filetype(FT_RAW)
        // , m_fd(-1)
        , m_fp(NULL)
        , m_item_size(0)
        , m_sample_rate(0)
        , m_length(0)
        , m_data_offset(0)
        , m_freq_hint(freq_hint)
        , m_freq(0)
      {
        memset(&m_wfx, 0x00, sizeof(m_wfx));
        memset(&m_auxi, 0x00, sizeof(m_auxi));
        memset(&m_time_start, 0x00, sizeof(m_time_start));
        memset(&m_time_end, 0x00, sizeof(m_time_end));

        _open();
      }
      ~InputFile()
      {
        close();
      }
      bool _open()
      {
        std::ifstream f(m_path.c_str(), std::ifstream::in | std::ifstream::binary);
        if (f.is_open() == false)
        {
          throw std::runtime_error(std::string("failed to open: ") + m_path);
          // return false;
        }

        f.seekg(0, std::ios_base::end);
        m_length = f.tellg();
        m_data_offset = 0;
        f.seekg(0);

        uint32_t riff = 0;
        f.read((char*)&riff, sizeof(riff));
        if (htonl(riff) == FOURCC('R', 'I', 'F', 'F'))
        {
          uint32_t riff_size = 0;
          f.read((char*)&riff_size, sizeof(riff_size));

          uint32_t wave = 0;
          f.read((char*)&wave, sizeof(wave));
          if (htonl(wave) == FOURCC('W', 'A', 'V', 'E'))
          {
            m_filetype = FT_WAVE;

            while (f.good())
            {
              uint32_t chunk_id = 0;
              f.read((char*)&chunk_id, sizeof(chunk_id));
              chunk_id = htonl(chunk_id);
              uint32_t chunk_size = 0;
              f.read((char*)&chunk_size, sizeof(chunk_size));
              // chunk_size = htonl(chunk_size);

              size_t pos = f.tellg();

              if (chunk_id == FOURCC('f', 'm', 't', ' '))
              {
                if (chunk_size < sizeof(WAVE_FORMAT_EX))
                  throw std::runtime_error("invalid fmt chunk");

                f.read((char*)&m_wfx, sizeof(m_wfx));

                m_sample_rate = m_wfx.sample_rate; // FIXME: Check these and throw is invalid/not found
                m_item_size = m_wfx.channels * (m_wfx.bits_per_sample / 8);
              }
              else if (chunk_id == FOURCC('a', 'u', 'x', 'i'))
              {
                if (chunk_size < sizeof(AUXI))
                  throw std::runtime_error("invalid auxi chunk");

                f.read((char*)&m_auxi, sizeof(m_auxi));

                m_freq = m_auxi.freq1;
                m_time_start = m_auxi.time_start;
                m_time_end = m_auxi.time_end;
              }
              else if (chunk_id == FOURCC('d', 'a', 't', 'a'))
              {
                m_data_offset = pos;
              }
              else
              {
                // Ignore
              }

              f.seekg(pos + chunk_size);
            }
          }
          else
          {
            throw std::runtime_error(std::string("unknown RIFF type: " + fourcc_to_string(wave)));
          }
        }
        else
        {
          m_item_size = m_item_size_hint;
          m_sample_rate = m_sample_rate_hint;
          m_freq = m_freq_hint;
        }

        m_length -= m_data_offset;

        ///////////////////////////////////////////////////////////////////////

        m_timing_info.clear();  // should already be empty - '_open' only once currently

        if (m_path_timing.empty() == false)
        {
          std::ifstream timing_file(m_path_timing);

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
                double rate = atof(str.substr(1).c_str());
                if (rate <= 0.0)
                {
                  // fprintf(stderr, "[%s<%ld>] Invalid rate line: %s\n", name().c_str(), unique_id(), str.c_str());
                  throw std::runtime_error("invalid rate in timing file");
                }

                m_sample_rate = rate;

                continue;
              }

              int idx = str.find(',');
              if (idx == -1)
                continue;

              uint64_t ticks = atol(str.substr(0, idx).c_str());
              uint64_t sample_count = atol(str.substr(idx + 1).c_str());

              m_timing_info.push_back(std::make_pair(ticks, sample_count));

              last_sample_count = sample_count;
            }

            // FIXME: Update m_time_start/m_time_end

            // if (m_timing_info.size() >= 2)
            // {
            //   uint64_t last_count = d_length - last_sample_count;

            //   uint64_t tick_diff = m_timing_info[m_timing_info_new.size() - 1].first - m_timing_info_new[0].first;

            //   d_length_new = tick_diff + last_count;

            //   ///////////////////////////

            //   // uint64_t total_samples = d_timing_info_new[1].first - d_timing_info_new[0].first;

            //   // d_samples = d_timing_info_new[1].second - d_timing_info_new[0].second;
            //   // d_pad_count = total_samples - d_samples;
            // }

            // fprintf(stderr, "[%s<%ld>] Read %lu timing points from: %s (rate: %f, length: %llu)\n", name().c_str(), unique_id(), d_timing_info_new.size(), timing_filename, d_rate_new, d_length_new);
          }
          else
          {
            // fprintf(stderr, "[%s<%ld>] Failed to open: %s\n", name().c_str(), unique_id(), timing_filename);
            throw std::runtime_error(std::string("cannot open timing file: ") + m_path_timing);
          }
        }

        if (m_timing_info.empty())
        {
          m_timing_info.push_back(std::make_pair((uint64_t)0, (uint64_t)0));
        }

        ///////////////////////////////////////////////////////////////////////

        return true;
      }
      std::string stringify() const
      {
        // std::stringstream ss;

        // ss << m_path << " ("
        //   << "type: " << m_filetype << ", "
        //   << "item size: " << m_item_size << ", "
        //   << "samples: " << samples() << ", "
        //   << "sample rate: " << m_sample_rate << " Hz, "
        //   << "freq: " << m_freq << " Hz, "
        //   << "start time: " << m_time_start.year << "-" << m_time_start.month << "-" << m_time_start.day << " " << m_time_start.hour << ":" << m_time_start.minute << ":" << m_time_start.second << "." << m_time_start.millisecond << ", "
        //   << "end time: " << m_time_end.year << "-" << m_time_end.month << "-" << m_time_end.day << " " << m_time_end.hour << ":" << m_time_end.minute << ":" << m_time_end.second << "." << m_time_end.millisecond << ")";

        // return ss.str();

        return boost::str(boost::format("%s (type: %d, item size: %lu, samples: %llu (raw: %llu), sample rate: %f Hz, freq: %f Hz, timing points: %lu, start time: %04u-%02u-%02u %02u:%02u:%02u.%04u, end time: %04u-%02u-%02u %02u:%02u:%02u.%04u)")
          % m_path
          % m_filetype  // FIXME: To string
          % m_item_size
          % samples() % samples(true)
          % m_sample_rate
          % m_freq
          % m_timing_info.size()
          % m_time_start.year % m_time_start.month % m_time_start.day % m_time_start.hour % m_time_start.minute % m_time_start.second % m_time_start.millisecond
          % m_time_end.year % m_time_end.month % m_time_end.day % m_time_end.hour % m_time_end.minute % m_time_end.second % m_time_end.millisecond
          );
      }
      /*int*/FILE* open()
      {
        // if (m_fd == -1)
        if (m_fp == NULL)
        {
          int m_fd = ::open(m_path.c_str(), O_RDONLY | OUR_O_LARGEFILE | OUR_O_BINARY);
          if (m_fd < 0)
          {
            perror("failed to open handle");
            throw std::runtime_error(std::string("failed to open handle: " + m_path));
          }

          m_fp = fdopen(m_fd, "rb");
          if (m_fp == NULL)
          {
            perror("failed to open file");
            throw std::runtime_error(std::string("failed to open file: " + m_path));
          }
        }
        
        if (fseek(m_fp, m_data_offset, SEEK_SET) < 0)
          perror("failed to seek");

        // return m_fd;
        return m_fp;
      }
      int read(size_t len, void* p)
      {
        if (m_fp == NULL)
          return -1;
        return fread(p, m_item_size, len, m_fp); // FIXME: Assumes data continues to EOF
      }
      /*size_t*/int seek(size_t pos)
      {
        if (m_fp == NULL)
          return -1;
        size_t _pos = (pos * m_item_size) + m_data_offset;
        if (fseek(m_fp, _pos, SEEK_SET) < 0)
        {
          perror("failed to seek");
          return -1;
        }
        // return ftell(m_fp);
        return 0;
      }
      size_t tell()
      {
        if (m_fp == NULL)
          return 0;
        return ((ftell(m_fp) - m_data_offset) / m_item_size);
      }
      size_t samples(bool raw = false) const
      {
        if (m_item_size == 0)
          return 0;
        if ((raw) || (m_timing_info.size() < 2))
          return (m_length / m_item_size);
        uint64_t last_count = samples(true) - m_timing_info[m_timing_info.size() - 1].second;
        uint64_t tick_diff = m_timing_info[m_timing_info.size() - 1].first - m_timing_info[0].first;
        return tick_diff + last_count;
      }
      double duration() const
      {
        if (m_sample_rate == 0)
          return 0;
        return ((double)samples() / m_sample_rate);
      }
      void close()
      {
        if (m_fp != NULL)
        {
          fclose(m_fp); // Assuming this also closes FD
          m_fp = NULL;
        }
        // if (m_fd > -1)
        // {
        //   ::close(m_fd);
        //   m_fd = -1;
        // }
      }
      double freq() const
      {
        return m_freq;
      }
      const SYSTEM_TIME& start_time() const
      {
        return m_time_start;
      }
      const SYSTEM_TIME& end_time() const
      {
        return m_time_end;
      }
      const std::vector<TimingPair>& timing_info() const
      {
        return m_timing_info;
      }
      double sample_rate() const
      {
        return m_sample_rate;
      }
      const std::string& path() const
      {
        return m_path;
      }
    private:
      FileType m_filetype;
      // int m_fd;
      FILE* m_fp;
      size_t m_item_size, m_item_size_hint;
      double m_sample_rate, m_sample_rate_hint;
      double m_freq, m_freq_hint;
      std::string m_path, m_path_timing;
      size_t m_length; // Bytes
      size_t m_data_offset; // Bytes;
      WAVE_FORMAT_EX m_wfx;
      AUXI m_auxi;
      SYSTEM_TIME m_time_start, m_time_end;
      std::vector<TimingPair> m_timing_info;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class file_source_impl : public file_source
    {
    private:
      size_t d_itemsize;
      // FILE *d_fp;
      // FILE *d_new_fp;
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
      std::vector<std::string> d_files, d_files_new;
      std::vector<InputFile::sptr> d_input_files, d_input_files_new;
      std::vector<uint64_t> d_input_files_offsets, d_input_files_offsets_new;
      int d_current_file_idx;

      bool do_update();
      bool calculate_offset(uint64_t offset, uint64_t& file_offset, uint64_t& ticks, uint64_t& samples_left, uint64_t& pad_left, int& timing_idx);
      void tag(int offset, uint64_t ticks = -1);

    public:
      file_source_impl(size_t itemsize, const char *filename, bool repeat = false, long offset = 0, const char *timing_filename = NULL, bool pad = false, double rate = 0.0, bool auto_load = true, const std::vector<std::string>& files = std::vector<std::string>());
      ~file_source_impl();

      bool seek(long seek_point, int whence);
      bool seek(long seek_point) { return seek(seek_point, SEEK_SET); }
      void open(const char *filename, bool repeat = false, long offset = 0, const char *timing_filename = NULL, bool pad = false, double rate = 0.0, bool auto_load = true, const std::vector<std::string>& files = std::vector<std::string>());
      void close();
      size_t offset();
      size_t file_offset();
      double time(bool relative = false, bool raw = false);
      double sample_rate();
      double sample_count(bool raw = false);
      double duration();
      size_t file_index();
      std::string file_path();
      std::vector<std::string> files();

      int work(int noutput_items,
         gr_vector_const_void_star &input_items,
         gr_vector_void_star &output_items);
    };

    file_source::sptr file_source::make(size_t itemsize, const char *filename, bool repeat/* = false*/, long offset/* = 0*/, const char *timing_filename/* = NULL*/, bool pad/* = false*/, double rate/* = 0.0*/, bool auto_load/* = true*/, const std::vector<std::string>& files/* = std::vector<std::string>()*/)
    {
      return gnuradio::get_initial_sptr(new file_source_impl(itemsize, filename, repeat, offset, timing_filename, pad, rate, auto_load, files));
    }

    file_source_impl::file_source_impl(size_t itemsize, const char *filename, bool repeat/* = false*/, long offset/* = 0*/, const char *timing_filename/* = NULL*/, bool pad/* = false*/, double rate/* = 0.0*/, bool auto_load/* = true*/, const std::vector<std::string>& files/* = std::vector<std::string>()*/)
      : sync_block("file_source",
          io_signature::make(0, 0, 0),
          io_signature::make(1, 1, itemsize)),
      d_itemsize(itemsize),
      // d_fp(0),
      // d_new_fp(0),
      d_updated(false),
      d_rate(1.0),
      d_seeked(false),
      d_pad(pad),
      d_current_file_idx(-1)
    {
      fprintf(stderr, "[%s<%ld>] item size: %lu, file: %s, repeat: %s, offset: %ld, timing file: %s, pad: %s, force rate: %f, auto-load: %s, files count: %lu\n", name().c_str(), unique_id(), itemsize, filename, (repeat ? "yes" : "no"), offset, timing_filename, (pad ? "yes" : "no"), rate, (auto_load ? "yes" : "no"), files.size());

      open(filename, repeat, offset, timing_filename, pad, rate, auto_load, files);

      do_update();
    }

    file_source_impl::~file_source_impl()
    {
      // if(d_fp)
      //   fclose ((FILE*)d_fp);
      // if(d_new_fp)
      //   fclose ((FILE*)d_new_fp);

      // Not necessary as std::vector DTOR will take care of all files
      // if (d_current_file_idx > -1)
      //   d_input_files[d_current_file_idx].close();
    }

    bool file_source_impl::calculate_offset(uint64_t offset, uint64_t& file_offset, uint64_t& ticks, uint64_t& samples_left, uint64_t& pad_left, int& timing_idx)
    {
      if (offset > d_length)
      {
        fprintf(stderr, "[%s<%ld>] Tried to seek off end: %lld (length: %lld)\n", name().c_str(), unique_id(), offset, d_length);
        return false;
      }

      // if (d_timing_info.empty())  // Don't need this as no timing file will auto add one entry: (0, 0)
      // {
      //   file_offset = offset;
      //   ticks = (uint64_t)((double)offset * d_rate);
      //   samples_left = d_length - offset;
      //   pad_left = 0;
      //   timing_idx = -1;  // FIXME: Check this
      //   return true;
      // }

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

    bool file_source_impl::seek(long seek_point, int whence) // 'seek_point' in items
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

      if (file_offset > d_input_files_offsets[d_input_files_offsets.size() - 1])
        throw new std::runtime_error("error calculating file offset");

      size_t _file_idx = 0;
      uint64_t offset_adj = 0;
      for (; _file_idx < d_input_files_offsets.size(); ++_file_idx)
      {
        if (file_offset < d_input_files_offsets[_file_idx])
        {
          if (_file_idx > 0)
            offset_adj = d_input_files_offsets[_file_idx - 1];
          break;
        }
      }

      int file_idx = (int)_file_idx;

      if (file_idx != d_current_file_idx)
      {
        if (d_current_file_idx >= 0)
          d_input_files[d_current_file_idx]->close();

        fprintf(stderr, "[%s<%ld>] Switching to file %d: %s (offset adjust: %llu)\n", name().c_str(), unique_id(), (file_idx + 1), d_input_files[file_idx]->path().c_str(), offset_adj);

        d_input_files[file_idx]->open();
        d_current_file_idx = file_idx;
      }

      // int res = fseek((FILE*)d_fp, (file_offset * d_itemsize), SEEK_SET);
      uint64_t _file_offset = file_offset - offset_adj;
      int res = d_input_files[file_idx]->seek(_file_offset);
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

      // if (d_fp == NULL)
      //   return 0;

      // return ftell(d_fp);

      if (d_current_file_idx < 0)
        return 0;

      size_t offset = (d_current_file_idx == 0 ? 0 : d_input_files_offsets[d_current_file_idx - 1]);

      return (offset + d_input_files[d_current_file_idx]->tell());
    }

    double file_source_impl::time(bool relative/* = false*/, bool raw/* = false*/)
    {
      scoped_lock lock(fp_mutex);

      // fprintf(stderr, "[%s<%ld>] Getting time: file offset: %d, absolute offset: %llu\n", name().c_str(), unique_id(), (file_offset() / d_itemsize), d_offset);

      if (relative)
      {
        if (raw)
        {
          return (double)(file_offset()/* / d_itemsize*/) / d_rate;
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
          size_t offset = file_offset()/* / d_itemsize*/;
          return (double)(first_time + offset) / d_rate;
        }
        else
        {
          return (double)(first_time + d_offset) / d_rate;
        }
      }

      return 0.0; // Shouldn't ever get here
    }

    double file_source_impl::sample_rate()
    {
      return d_rate;
    }

    double file_source_impl::sample_count(bool raw/* = false*/)
    {
      return (raw ? d_file_length : d_length);
    }

    double file_source_impl::duration()
    {
      return sample_count() / sample_rate();
    }

    size_t file_source_impl::file_index()
    {
      return d_current_file_idx;
    }

    std::string file_source_impl::file_path()
    {
      return d_files[d_current_file_idx];
    }

    std::vector<std::string> file_source_impl::files()
    {
      return d_files;
    }

    void file_source_impl::open(const char *filename, bool repeat/* = false*/, long offset/* = 0*/, const char *timing_filename/* = NULL*/, bool pad/* = false*/, double rate/* = 0.0*/, bool auto_load/* = true*/, const std::vector<std::string>& files/* = std::vector<std::string>()*/)
    {
      //if ((filename == NULL) || (filename[0] == '\0'))
      //  return;
      std::vector<std::string> _files(files);
      if ((filename != NULL) && (strlen(filename) > 0))
        _files.insert(_files.begin(), filename);

      if (_files.empty())
      {
        throw std::runtime_error("no files supplied");
        // return;
      }

      std::vector<InputFile::sptr> _input_files;
      std::vector<uint64_t> _input_files_offsets;
      uint64_t cumulative_samples = 0, cumulative_samples_raw = 0;
      std::string _timing_filename;
      if ((timing_filename != NULL) && (timing_filename[0] != '\0'))
        _timing_filename = timing_filename;
      else if (auto_load)
      {
        std::string test_timing_file = _files[0] + ".timing";
        std::ifstream timing_file(test_timing_file);

        if (timing_file.is_open())
          _timing_filename = test_timing_file;
      }

      double _rate = rate;

      std::vector<TimingPair> _timing_info;

      for (size_t i = 0; i < _files.size(); i++)
      {
        const std::string& path = _files[i];
        std::string timing_file_path;
        if (i == 0)
          timing_file_path = _timing_filename; // Might be empty, populated, or auto-loaded
        else if (_timing_filename.empty() == false) // If > 1 file and first timing file was loaded (either by auto-load or populated)
          timing_file_path = path + ".timing";
        InputFile::sptr input_file = InputFile::sptr(new InputFile(path, d_itemsize, rate, 0.0, timing_file_path));
        if (i == 0)
        {
          _rate = input_file->sample_rate();
          _timing_info.push_back(input_file->timing_info()[0]);  // Only take first point from first file
        }
        // FIXME: Check first point in other files to ensure it didn't coincide with dropped samples, otherwise include the point!
        for (size_t j = 1; j < input_file->timing_info().size(); ++j)  // Take all but first point from all files
        {
          // FIXME: Check time doesn't go backwards
          TimingPair tp = input_file->timing_info()[j];
          tp.second += cumulative_samples_raw;
          _timing_info.push_back(tp);
        }
        std::cout << (boost::format("[%03lu] %s") % (i + 1) % input_file->stringify()) << std::endl;
        _input_files.push_back(input_file);
        cumulative_samples += input_file->samples(); // Includes timing info (if loaded)
        cumulative_samples_raw += input_file->samples(true);
        _input_files_offsets.push_back(cumulative_samples);

        if (auto_load)
        {
#ifdef _WIN32
          static const char sep = '\\';
#else
          static const char sep = '/';
#endif // _WIN32
          size_t filename_idx = path.rfind(sep);
          std::string dirname;
          std::string filename(path);
          if (filename_idx != std::string::npos)
          {
            filename = path.substr(filename_idx + 1);
            dirname = path.substr(0, filename_idx);
          }
          size_t idx = filename.find('_');
          std::string front;
          if (idx != std::string::npos)
            front = filename.substr(0, idx + 1);
          idx = filename.rfind('_');
          std::string back;
          if (idx != std::string::npos)
            back = filename.substr(idx);
          std::string datetime_str = boost::str(boost::format("%04u%02u%02u_%02u%02u%02uZ")
            % input_file->end_time().year
            % input_file->end_time().month
            % input_file->end_time().day
            % input_file->end_time().hour
            % input_file->end_time().minute
            % input_file->end_time().second);
          std::string freq_str(std::to_string((uint64_t)(input_file->freq() / 1e3)) + "kHz");
          std::string next_file_path(dirname + sep + front + datetime_str + "_" + freq_str + back);
          std::ifstream next_file(next_file_path, std::ifstream::in | std::ifstream::binary);
          if (next_file.is_open())
          {
            if (std::find(_files.begin(), _files.end(), next_file_path) == _files.end())
              _files.insert(_files.begin() + i + 1, next_file_path);
            else
              std::cout << "Already in file list: " << next_file_path << std::endl;
          }
          else
          {
            std::cout << "Next file not found: " << next_file_path << std::endl;
          }
        }
      }

      double _new_rate = ((_rate <= 0.0) ? 1.0 : _rate);

      fprintf(stderr, "[%s<%ld>] Opened %lu files: total samples: %llu, total timing points: %lu, rate: %f\n", name().c_str(), unique_id(), _files.size(), cumulative_samples, _timing_info.size(), _new_rate);

      /////////////////////////////////////////////////////////////////////////

      scoped_lock lock(fp_mutex); // obtain exclusive access for duration of this function

      d_files_new = _files;
      d_input_files_new = _input_files;
      d_input_files_offsets_new = _input_files_offsets;

      // int fd;

      // // we use "open" to use to the O_LARGEFILE flag
      // if ((fd = ::open(filename, O_RDONLY | OUR_O_LARGEFILE | OUR_O_BINARY)) < 0)
      // {
      //   perror(filename);
      //   throw std::runtime_error("can't open file");
      // }

      // if (d_new_fp)
      // {
      //   fclose(d_new_fp);
      //   d_new_fp = 0;
      // }

      // if ((d_new_fp = fdopen (fd, "rb")) == NULL)
      // {
      //   perror(filename);
      //   ::close(fd);  // don't leak file descriptor if fdopen fails
      //   throw std::runtime_error("can't open file");
      // }

      d_pad_new = pad;
      d_repeat_new = repeat;

      // fseek(d_new_fp, 0, SEEK_END);
      // d_file_length_new = ftell(d_new_fp) / d_itemsize;
      d_file_length_new = cumulative_samples_raw;
      // d_length_new = d_file_length_new;
      d_length_new = cumulative_samples;

      // Shouldn't be done here:
      // d_samples = d_length_new;
      // d_pad_count = 0;

      // fseek(d_new_fp, 0, SEEK_SET);

      // fprintf(stderr, "[%s<%ld>] Opened: %s (%llu samples)\n", name().c_str(), unique_id(), filename, d_file_length_new);

      d_offset_new = offset;

      // d_timing_info_new.clear();
      d_timing_info_new = _timing_info;
      // d_rate_new = ((rate <= 0.0) ? 1.0 : rate);
      d_rate_new = _new_rate; // First file sample_rate takes precedence over hint

      // FIXME: Handle when file doesn't match timing info (e.g. from a loop)

      // if ((timing_filename != NULL) && (timing_filename[0] != '\0'))
      // {
      //   std::ifstream timing_file(timing_filename);

      //   if (timing_file.is_open())
      //   {
      //     std::string str;

      //     uint64_t last_sample_count = -1;

      //     while (true)
      //     {
      //       timing_file >> str;
      //       if (timing_file.good() == false)
      //         break;

      //       boost::trim(str);
            
      //       if (str.empty())
      //         continue;

      //       if (str[0] == '#')
      //         continue;

      //       if (toupper(str[0]) == 'R')
      //       {
      //         d_rate_new = atof(str.substr(1).c_str());
      //         if (d_rate_new <= 0.0)
      //         {
      //           fprintf(stderr, "[%s<%ld>] Invalid rate line: %s\n", name().c_str(), unique_id(), str.c_str());
      //           throw std::runtime_error("invalid rate in timing file");

      //           d_rate_new = 1.0;
      //         }

      //         continue;
      //       }

      //       int idx = str.find(',');
      //       if (idx == -1)
      //         continue;

      //       uint64_t ticks = atol(str.substr(0, idx).c_str());
      //       uint64_t sample_count = atol(str.substr(idx + 1).c_str());

      //       d_timing_info_new.push_back(std::make_pair(ticks, sample_count));

      //       last_sample_count = sample_count;
      //     }

      //     if (d_timing_info_new.size() >= 2)
      //     {
      //       uint64_t last_count = d_length_new - last_sample_count;

      //       uint64_t tick_diff = d_timing_info_new[d_timing_info_new.size() - 1].first - d_timing_info_new[0].first;
            
      //       d_length_new = tick_diff + last_count;

      //       ///////////////////////////

      //       // uint64_t total_samples = d_timing_info_new[1].first - d_timing_info_new[0].first;

      //       // d_samples = d_timing_info_new[1].second - d_timing_info_new[0].second;
      //       // d_pad_count = total_samples - d_samples;
      //     }

      //     fprintf(stderr, "[%s<%ld>] Read %lu timing points from: %s (rate: %f, length: %llu)\n", name().c_str(), unique_id(), d_timing_info_new.size(), timing_filename, d_rate_new, d_length_new);
      //   }
      //   else
      //   {
      //     fprintf(stderr, "[%s<%ld>] Failed to open: %s\n", name().c_str(), unique_id(), timing_filename);
      //     throw std::runtime_error("cannot open timing file");
      //   }
      // }

      // if (d_timing_info_new.empty())
      // {
      //   d_timing_info_new.push_back(std::make_pair((uint64_t)0, (uint64_t)0));
      // }

      d_updated = true;
    }

    void file_source_impl::close()
    {
      // obtain exclusive access for duration of this function
      scoped_lock lock(fp_mutex);

      // if (d_new_fp != NULL)
      // {
      //   fclose(d_new_fp);
      //   d_new_fp = NULL;
      // }

      // FIXME: Move to '_reset' function
      d_input_files.clear();
      d_files.clear();
      d_input_files_offsets.clear();
      d_current_file_idx = -1;

      d_updated = true;
    }

    bool file_source_impl::do_update()
    {
      if (d_updated)
      {
        scoped_lock lock(fp_mutex); // hold while in scope

        // if (d_fp)
        //   fclose(d_fp);

        // d_fp = d_new_fp;    // install new file pointer
        // d_new_fp = 0;

        d_files = d_files_new;
        d_input_files = d_input_files_new;
        d_input_files_new.clear(); // Decrease ref count on shared pointers
        d_input_files_offsets = d_input_files_offsets_new;

        d_current_file_idx = -1;
        // d_current_file_idx = 0;
        // /*d_fp = */d_input_files[d_current_file_idx]->open();

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
          throw std::runtime_error("failed to seek during update");

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

    int file_source_impl::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    {
      char *o = (char*)output_items[0];
      int size = noutput_items;

      bool updated = do_update();

      // if (d_fp == NULL)
      if (d_current_file_idx < 0)
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
        // int i = fread(o, d_itemsize, size, (FILE*)d_fp);
        int i = d_input_files[d_current_file_idx]->read(size, o); // Might not return all data if hitting EOF in current file

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

        size_t remaining = d_input_files[d_current_file_idx]->samples(true) - d_input_files[d_current_file_idx]->tell();
        if (remaining == 0)
        {
          fprintf(stderr, "[%s<%ld>] Reached EOF in file: %d\n", name().c_str(), unique_id(), (d_current_file_idx + 1));

          if (d_current_file_idx < (d_input_files.size() - 1))
          {
            if (seek(d_input_files_offsets[d_current_file_idx]) == false)
              throw new std::runtime_error("failed to seek to next file");

            d_seeked = false; // FIXME: Check that this didn't coincide with dropped samples!

            continue;
          }
          else
          {
            fprintf(stderr, "[%s<%ld>] Short read at EOF of last file: %d of %d\n", name().c_str(), unique_id(), i, (size + i));

            // Fall through to EOF
          }
        }
        else if (i > 0)     // short read, try again
        {
          fprintf(stderr, "[%s<%ld>] Short read: %d of %d with %lu samples remaining in file %d\n", name().c_str(), unique_id(), i, (size + i), remaining, (d_current_file_idx + 1));

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

      if (size > 0) // EOF or error
      {
        if (size == noutput_items)       // we didn't read anything; say we're done
          return -1;

        return (noutput_items - size);  // else return partial result
      }

      return noutput_items;
    }

  } /* namespace baz */
} /* namespace gr */
