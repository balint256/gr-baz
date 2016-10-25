/* -*- c++ -*- */
/*
 * Copyright 2004,2013 Free Software Foundation, Inc.
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

/*
 * gr-baz by Balint Seeber (http://spench.net/contact)
 * Information, documentation & samples: http://wiki.spench.net/wiki/gr-baz
 */

#ifndef INCLUDED_BAZ_CORRELATOR_H
#define INCLUDED_BAZ_CORRELATOR_H

#include <gnuradio/sync_block.h>

class BAZ_API baz_correlator;

/*
 * We use boost::shared_ptr's instead of raw pointers for all access
 * to gr::blocks (and many other data structures).  The shared_ptr gets
 * us transparent reference counting, which greatly simplifies storage
 * management issues.  This is especially helpful in our hybrid
 * C++ / Python system.
 *
 * See http://www.boost.org/libs/smart_ptr/smart_ptr.htm
 *
 * As a convention, the _sptr suffix indicates a boost::shared_ptr
 */
typedef boost::shared_ptr<baz_correlator> baz_correlator_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_correlator.
 *
 * To avoid accidental use of raw pointers, baz_correlator's
 * constructor is private.  baz_make_correlator is the public
 * interface for creating new instances.
 */
BAZ_API baz_correlator_sptr baz_make_correlator (
    float samp_rate,
    float symbol_rate,
    int window_length,
    float threshold=0.5,
    int width=1024,
    const char* sync_path="sync.dat",
    int sync_length=511,
    int sync_offset=50,
    //sync_dtype='c8',
    int sync_window_length=500
);

/*!
 * \brief square2 a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr::sync_block.
 */
class BAZ_API baz_correlator : public gr::block
{
private:
    // The friend declaration allows baz_make_correlator To
    // access the private constructor.

    friend BAZ_API baz_correlator_sptr baz_make_correlator(
        float samp_rate,
        float symbol_rate,
        int window_length,
        float threshold,
        int width,
        const char* sync_path,
        int sync_length,
        int sync_offset,
        //sync_dtype='c8',
        int sync_window_length
    );

    baz_correlator(
        float samp_rate,
        float symbol_rate,
        int window_length,
        float threshold,
        int width,
        const char* sync_path,
        int sync_length,
        int sync_offset,
        //sync_dtype='c8',
        int sync_window_length
);  	// private constructor

    float d_samp_rate;
    float d_symbol_rate;
    int d_window_length;
    float d_threshold;
    int d_width;
    //const char* sync_path;
    //int sync_length;
    //int sync_offset;
    //sync_dtype='c8',
    int d_sync_window_length;

    std::vector<std::complex<float> > d_sync;
    bool d_synced;
    int64_t d_next_window_idx;
    int64_t d_current_idx;
    std::vector<std::complex<float> > d_conjmul_result;
    //std::vector<float> d_abs_result;
    float d_max_peak;
    int d_max_peak_idx;
    int d_sync_window_idx;
    int d_current_item_idx;

    std::complex<float> correlate(const std::complex<float>* in, const std::complex<float>* sync);

public:
    ~baz_correlator();	// public destructor

    //

    //void forecast(int noutput_items, gr_vector_int &ninput_items_required);
    int general_work(int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_CORRELATOR_H */
