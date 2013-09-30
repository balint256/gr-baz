/* -*- c++ -*- */
/*
 * Copyright 2011 Communications Engineering Lab, KIT
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

#ifndef INCLUDED_BAZ_MUSIC_DOA_H
#define INCLUDED_BAZ_MUSIC_DOA_H

#include <gnuradio/sync_block.h>
#include <vector>
#include <armadillo>
#include <gnuradio/thread/thread.h>

class baz_music_doa;
typedef boost::shared_ptr<baz_music_doa> baz_music_doa_sptr;

typedef std::vector<gr_complex> antenna_response_t;
typedef std::vector<antenna_response_t> array_response_t;
typedef std::pair<double,double> doa_t;

baz_music_doa_sptr baz_make_music_doa(unsigned int m, unsigned int n, unsigned int nsamples, const array_response_t& array_response, unsigned int resolution);

class baz_music_doa : public gr::sync_block
{
private:
	friend baz_music_doa_sptr baz_make_music_doa(unsigned int m, unsigned int n, unsigned int nsamples, const array_response_t& array_response, unsigned int resolution);

	baz_music_doa(unsigned int m, unsigned int n, unsigned int nsamples, const array_response_t& array_response, unsigned int resolution);

public:
	~baz_music_doa();

	int work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);

private:
	unsigned int d_m;
	unsigned int d_n;
	unsigned int d_nsamples;
	array_response_t d_array_response;
	unsigned int d_resolution;
	gr::thread::mutex  d_mutex;

public:
	void set_array_response(const array_response_t& array_response);
};

#endif /* INCLUDED_BAZ_MUSIC_DOA_H */
