/* -*- c++ -*- */
/*
 * Copyright 2004 Free Software Foundation, Inc.
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

#ifndef INCLUDED_BAZ_BURSTER_CONFIG_H
#define INCLUDED_BAZ_BURSTER_CONFIG_H

typedef struct baz_burster_config_t
{
	int sample_rate;		// Hz
	int item_size;			// bytes
	int burst_length;		// # items (= vlen)
	double interval;		// seconds|samples
	bool sample_interval;	// false: interval is seconds, true: interval is # samples
	bool relative_time;		// false: absolute time (calculate from absolute burst time), true: calculate from when last burst was transmitted
	bool drop_current;		// false: hold current message until queue can accept, true: drop current and try with future burst
	bool use_host_time;		// false: derive time from stream, true: use wall time
	bool read_time_tag;		// false: derive time only from stream, true: derive time from time tag AND sample count
	bool output_messages;	// output bursts as messages
	gr::msg_queue::sptr msgq;	// message destination
	bool output_stream;		// output bursts on output stream
	bool trigger_on_tags;	// false: ignore tags, true: process tags
	bool use_tag_lengths;	// false: ignore lengths in tag, true: override burst_length with length in tag
	std::vector<std::string> trigger_tags;		// <sob>
	std::vector<std::string> length_tags;		// <length> -> contains 'int' with length in samples
	std::map<std::string,std::string> eob_tags;	// <sob,eob>
} baz_burster_config;

#endif /* INCLUDED_BAZ_BURSTER_CONFIG_H */
