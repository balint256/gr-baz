/* -*- c++ -*- */
/* 
 * Copyright 2014 Balint Seeber <balint256@gmail.com>.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */


#ifndef INCLUDED_BAZ_USRP_ACQUIRE_H
#define INCLUDED_BAZ_USRP_ACQUIRE_H

#include <boost/thread/mutex.hpp>
#include <vector>

#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/types/time_spec.hpp>

#include <gnuradio/uhd/usrp_source.h>

namespace gr {
  namespace baz {

    /*!
     * \brief <+description+>
     *
     */
    class BAZ_API usrp_acquire
    {
    public:
      usrp_acquire(::uhd::usrp::multi_usrp::sptr dev, const ::uhd::stream_args_t &stream_args);
      ~usrp_acquire();
    public:
      typedef boost::shared_ptr<usrp_acquire> sptr;
      static sptr make(::uhd::usrp::multi_usrp::sptr dev, const ::uhd::stream_args_t &stream_args);
      static sptr make_from_source(/*::gr::uhd::usrp_source::sptr*/::gr::basic_block_sptr source, const ::uhd::stream_args_t &stream_args);
    public:
      std::vector<size_t> finite_acquisition_v(const size_t nsamps, bool stream_now = true, double delay = 0.0, size_t skip = 0, double timeout = 1.0);
      void set_gpio_attr(const std::string &bank, const std::string &attr, const boost::uint32_t value, const boost::uint32_t mask, const size_t mboard = 0);
    private:
      ::uhd::usrp::multi_usrp::sptr m_dev;
      ::uhd::stream_args_t m_stream_args;
      ::boost::mutex d_mutex;
      ::uhd::rx_streamer::sptr m_rx_stream;
      size_t m_samps_per_packet;
      std::vector<std::vector<std::complex<float> >* > m_data;
    };

  } // namespace baz
} // namespace gr

#endif /* INCLUDED_BAZ_USRP_ACQUIRE_H */
