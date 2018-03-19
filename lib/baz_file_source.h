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

#ifndef INCLUDED_BAZ_FILE_SOURCE_H
#define INCLUDED_BAZ_FILE_SOURCE_H

#include <gnuradio/sync_block.h>

namespace gr {
  namespace baz {

    /*!
     * \brief Read stream from file
     * \ingroup file_operators_blk
     */
    class BAZ_API file_source : virtual public sync_block
    {
    public:

      // gr::blocks::file_source::sptr
      typedef boost::shared_ptr<file_source> sptr;

      /*!
       * \brief Create a file source.
       *
       * Opens \p filename as a source of items into a flowgraph. The
       * data is expected to be in binary format, item after item. The
       * \p itemsize of the block determines the conversion from bits
       * to items.
       *
       * If \p repeat is turned on, the file will repeat the file after
       * it's reached the end.
       *
       * \param itemsize	the size of each item in the file, in bytes
       * \param filename	name of the file to source from
       * \param repeat	repeat file from start
       */
      static sptr make(size_t itemsize, const char *filename, bool repeat = false, long offset = 0, const char *timing_filename = NULL, bool pad = false, double rate = 0.0, bool auto_load = true, const std::vector<std::string>& files = std::vector<std::string>());

      /*!
       * \brief seek file to \p seek_point relative to \p whence
       *
       * \param seek_point	sample offset in file
       * \param whence	one of SEEK_SET, SEEK_CUR, SEEK_END (man fseek)
       */
      virtual bool seek(long seek_point) = 0;
      virtual bool seek(long seek_point, int whence) = 0;

      /*!
       * \brief Opens a new file.
       *
       * \param filename	name of the file to source from
       * \param repeat	repeat file from start
       */
      virtual void open(const char *filename, bool repeat = false, long offset = 0, const char *timing_filename = NULL, bool pad = false, double rate = 0.0, bool auto_load = true, const std::vector<std::string>& files = std::vector<std::string>()) = 0;

      /*!
       * \brief Close the file handle.
       */
      virtual void close() = 0;

      virtual size_t offset() = 0;
      virtual size_t file_offset() = 0;
      virtual double time(bool relative = false, bool raw = false) = 0;
      virtual double sample_rate() = 0;
      virtual double sample_count(bool raw = false) = 0;
      virtual double duration() = 0;
      virtual size_t file_index() = 0;
      virtual std::string file_path() = 0;
      virtual std::vector<std::string> files() = 0;
    };

  } /* namespace baz */
} /* namespace gr */

#endif /* INCLUDED_BAZ_FILE_SOURCE_H */
