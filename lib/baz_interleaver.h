/* -*- c++ -*- */
/*
 * Copyright 2007,2012 Free Software Foundation, Inc.
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

#ifndef INCLUDED_BAZ_INTERLEAVER_H
#define INCLUDED_BAZ_INTERLEAVER_H

#include <gnuradio/block.h>

namespace gr {
  namespace baz {

    /*!
     * \brief Detect the peak of a signal
     * \ingroup peak_detectors_blk
     *
     * \details
     * If a peak is detected, this block outputs a 1,
     * or it outputs 0's.
     */
    class BAZ_API interleaver : virtual public block
    {
    public:
      // gr::baz::interleaver::sptr
      typedef boost::shared_ptr<interleaver> sptr;

      static sptr make(int item_size, int vlen_in, int vlen_out, int out_trigger = 0, int output_ports = 1, bool top_down_in = false, bool vector_in = true, bool vector_out = true, bool verbose = false);

      //virtual void set_swap(bool swap) = 0;

      //virtual bool get_swap() const = 0;
    };

  } /* namespace baz */
} /* namespace gr */

#endif /* INCLUDED_BAZ_INTERLEAVER_H */
