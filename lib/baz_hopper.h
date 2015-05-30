/* -*- c++ -*- */
/*
 * Copyright 2007,2013 Free Software Foundation, Inc.
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

#ifndef INCLUDED_BAZ_HOPPER_H
#define INCLUDED_BAZ_HOPPER_H

#include <gnuradio/block.h>
#include <boost/thread.hpp>

#include <vector>

#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/types/time_spec.hpp>

#include <gnuradio/uhd/usrp_source.h>

class BAZ_API baz_hopper;
typedef boost::shared_ptr<baz_hopper> baz_hopper_sptr;

BAZ_API baz_hopper_sptr baz_make_hopper(
	size_t item_size,
	int sample_rate,
	int chunk_length,
	int drop_length,
	std::vector<std::vector<double> > freqs,
	::gr::basic_block_sptr source,
	bool verbose = false
);

/*!
 * \brief hop
 * \ingroup misc_blk
 */
class BAZ_API baz_hopper : public gr::block
{
	friend BAZ_API baz_hopper_sptr baz_make_hopper(
		size_t item_size,
		int sample_rate,
		int chunk_length,
		int drop_length,
		std::vector<std::vector<double> > freqs,
		::gr::basic_block_sptr source,
		bool verbose
	);

	baz_hopper(
		size_t item_size,
		int sample_rate,
		int chunk_length,
		int drop_length,
		std::vector<std::vector<double> > freqs,
		::gr::basic_block_sptr source,
		bool verbose = false
	);

	//boost::mutex d_mutex;
	size_t d_item_size;
	int d_sample_rate;
	int d_chunk_length, d_drop_length;
	std::vector<std::vector<double> > d_freqs;
	bool d_verbose;
	uint64_t d_last_time_seconds;
	double d_last_time_fractional_seconds;
	uint64_t d_time_offset;
	::uhd::usrp::multi_usrp::sptr d_dev;
	bool d_seen_time;
	std::deque<uint64_t> d_scheduled;
	std::map<uint64_t,uint64_t> d_dest;
	int d_chunk_counter;
	uhd::time_spec_t d_last_hop;
	std::vector<std::pair<double,int> > d_freq_dest;
	int d_freq_idx;
	int d_zero_counter;
	bool d_reset;

public:
	~baz_hopper();
	
	//int delay () const { return d_delay; }
	//void set_delay (int delay);

	void forecast(int noutput_items, gr_vector_int &ninput_items_required);
	int general_work(int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif // INCLUDED_BAZ_HOPPER_H
