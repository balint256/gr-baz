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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include <baz_usrp_acquire.h>

namespace gr {
  namespace baz {

    usrp_acquire::sptr usrp_acquire::make(::uhd::usrp::multi_usrp::sptr dev, const ::uhd::stream_args_t &stream_args)
    {
        return sptr(new usrp_acquire(dev, stream_args));
    }
    
    usrp_acquire::sptr usrp_acquire::make_from_source(::gr::basic_block_sptr source, const ::uhd::stream_args_t &stream_args)
    {
        ::gr::uhd::usrp_source::sptr usrp_src = boost::dynamic_pointer_cast< ::gr::uhd::usrp_source>(source);
        if (not usrp_src)
        {
            throw std::runtime_error("Could not dynamic cast to USRP source");
            //return usrp_acquire::sptr();
        }
        
        return sptr(new usrp_acquire(usrp_src->get_device(), stream_args));
    }
    
    usrp_acquire::usrp_acquire(::uhd::usrp::multi_usrp::sptr dev, const ::uhd::stream_args_t &stream_args)
        : m_dev(dev)
        , m_stream_args(stream_args)
        , m_samps_per_packet(0)
    {
    }
    
    usrp_acquire::~usrp_acquire()
    {
        for (size_t i = 0; i < m_data.size(); ++i)
            delete m_data[i];
        
        m_data.clear();
    }
    
    std::vector<size_t> usrp_acquire::finite_acquisition_v(const size_t nsamps, bool stream_now, double delay, size_t skip, double timeout)
    {
        boost::mutex::scoped_lock lock(d_mutex);
        
        if (m_rx_stream)
        {
            //this->stop(); // Does flush
        }
        else
        {
            m_rx_stream = m_dev->get_rx_stream(m_stream_args);
            m_samps_per_packet = m_rx_stream->get_max_num_samps();
        }
        
        size_t _nchan = m_stream_args.channels.size();
        
        // create a multi-dimensional container to hold an array of sample buffers
        //std::vector<std::vector<std::complex<float> > > samps(_nchan, std::vector<std::complex<float> >(nsamps));
        
        const size_t item_size = sizeof(std::complex<float>);
        
        // load the void* vector of buffer pointers
        std::vector<void *> buffs(_nchan);
        for(size_t i = 0; i < _nchan; i++)
        {
            if (i >= m_data.size())
                m_data.push_back(new std::vector<std::complex<float> >(nsamps));
            else if (m_data[i]->size() != nsamps)
                m_data[i]->resize(nsamps);
            
            buffs[i] = &(m_data[i]->front());
        }
        
        // tell the device to stream a finite amount
        ::uhd::stream_cmd_t cmd(::uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
        cmd.num_samps = nsamps;
        cmd.stream_now = stream_now;
        cmd.time_spec = m_dev->get_time_now() + ::uhd::time_spec_t(delay);
        
        for (size_t i = 0; i < _nchan; i++)
            m_dev->issue_stream_cmd(cmd, m_stream_args.channels[i]);
        
        ::uhd::rx_metadata_t rx_metadata;
        
        // receive samples until timeout
        const size_t actual_num_samps = m_rx_stream->recv(buffs, nsamps, rx_metadata, timeout);
        
        std::vector<size_t> res;
        
        size_t to_skip = std::min(actual_num_samps, skip);
        size_t final_num_samps = actual_num_samps - to_skip;
        
        // resize the resulting sample buffers
        for(size_t i = 0; i < _nchan; i++)
        {
            m_data[i]->resize(actual_num_samps);
            size_t ptr = (size_t)&(m_data[i]->front());
            ptr += (to_skip * item_size);
            res.push_back(ptr);
        }
        
        res.push_back(final_num_samps);
        
        return res;
    }
    
    void usrp_acquire::set_gpio_attr(const std::string &bank, const std::string &attr, const boost::uint32_t value, const boost::uint32_t mask, const size_t mboard/* = 0*/)
    {
        m_dev->set_gpio_attr(bank, attr, value, mask, mboard);
    }

  } /* namespace baz */
} /* namespace gr */
