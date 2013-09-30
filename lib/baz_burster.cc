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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <baz_burster.h>
#include <gnuradio/io_signature.h>

baz_burster_sptr baz_make_burster (const baz_burster_config& config)
{
  return baz_burster_sptr (new baz_burster (config));
}

static const int MIN_IN  = 1;	// mininum number of input streams
static const int MAX_IN  = 1;	// maximum number of input streams
static const int MIN_OUT = 0;	// minimum number of output streams
static const int MAX_OUT = 1;	// maximum number of output streams

baz_burster::baz_burster (const baz_burster_config& config)
  : gr::block ("baz_burster",
		gr::io_signature::make (MIN_IN,  MAX_IN,  config.item_size),
		gr::io_signature::make (MIN_OUT, MAX_OUT, config.item_size/* * config.vlen*/))	// FIXME: Another config variable with vlen to support implicit vector output
	, d_config(config)
	//, d_last_burst_sample_time(-1)
	//, d_last_burst_time(-1)
	//, d_samples_since_last_time_tag(-1)
{
	fprintf(stderr, "[%s<%i>] item size: %d, sample rate: %d, interval type: %s\n", name().c_str(), unique_id(), config.item_size, config.sample_rate, (d_config.sample_interval ? "samples" : "seconds"));
	
	memset(&d_dummy_zero_first, 0x00, offsetof(baz_burster,d_dummy_zero_last) - offsetof(baz_burster,d_dummy_zero_first));
	
	d_system_time_ticks_per_second = boost::posix_time::time_duration::ticks_per_second();
	
	d_stream_time.sample_rate = d_config.sample_rate;
	
	//set_history(1 + );
	//set_output_multiple();
	//set_relative_rate();
	
	set_burst_length(d_config.burst_length);
}

void baz_burster::set_burst_length(int length)
{
	d_message_buffer_length = d_config.item_size * length;
	if (d_message_buffer == NULL)
		d_message_buffer = (char*)malloc(d_message_buffer_length);
	else
		d_message_buffer = (char*)realloc(d_message_buffer, d_message_buffer_length);
	d_config.burst_length = length;
fprintf(stderr, "[%s<%i>] burst length: %i (%i bytes)\n", name().c_str(), unique_id(), length, d_message_buffer_length);
}

baz_burster::~baz_burster ()
{
	if (d_message_buffer != NULL)
		free(d_message_buffer);
		//delete [] d_message_buffer;
}
/*
+	int sample_rate;		// Hz
+	int item_size;			// bytes
+	int burst_length;		// # items (= vlen)
+	double interval;		// seconds|samples
+	bool sample_interval;	// false: interval is seconds, true: interval is # samples
+	bool relative_time;		// false: absolute time (calculate from absolute burst time), true: calculate from when last burst was transmitted
+	bool drop_current;		// false: hold current message until queue can accept, true: drop current and try with future burst
+	bool use_host_time;		// false: derive time from stream, true: use wall time
+	bool read_time_tag;		// false: derive time only from stream, true: derive time from time tag AND sample count
+	bool output_messages;	// output bursts as messages
+	gr::msg_queue::sptr msgq;	// message destination
+	bool output_stream;		// output bursts on output stream
+	bool trigger_on_tags;	// false: ignore tags, true: process tags
	bool use_tag_lengths;	// false: ignore lengths in tag, true: override burst_length with length in tag
	std::vector<std::string> trigger_tags;		// <sob>
	std::vector<std::string> length_tags;		// <length> -> contains 'int' with length in samples
	std::map<std::string,std::string> eob_tags;	// <sob,eob>
*/
static const pmt::pmt_t RX_TIME_KEY = pmt::string_to_symbol("rx_time");

void baz_burster::forecast(int noutput_items, gr_vector_int &ninput_items_required)
{
	//ninput_items_required[0] = noutput_items;
	
	gr::block::forecast(noutput_items, ninput_items_required);
}

int baz_burster::general_work (int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
	const uint64_t read_before_this_work = nitems_read(0);
	const int input_count = ninput_items[0];
	
	int items_produced = 0;
	
	const char* in = (const char*)input_items[0];
	char* out = NULL;
	if (output_items.size() > 0)
		out = (char*)output_items[0];
	
	////////////////////////////////////
	
	//if ((d_config.output_messages == false) && (d_current_msg))	// FIXME: Perhaps do this in set function
	//	d_current_msg.reset();
	
	////////////////////////////////////
	
	const int time_tag_port = 0;
	
	int next_time_tag_index = -1;
	uint64_t next_time_tag_offset = 0;
	//int time_tag_sample_offset = 0;
	
	if (d_config.read_time_tag)
	{
		d_incoming_time_tags.clear();
		
		get_tags_in_range(d_incoming_time_tags, time_tag_port, read_before_this_work, (read_before_this_work + input_count), RX_TIME_KEY);
		if (d_incoming_time_tags.size() > 0)
		{
			next_time_tag_index = 0;
			next_time_tag_offset = d_incoming_time_tags[0].offset;
		}
	}
	
	////////////////////////////////////
	
	if (d_config.trigger_on_tags)
	{
		// FIXME
		//d_config.use_tag_lengths
		//d_config.trigger_tags
		//d_config.length_tags
		//d_config.eob_tags
	}
	
	////////////////////////////////////
	
	for (int i = 0; i < input_count; ++i)
	{
		const uint64_t input_sample_index = read_before_this_work + i;
		
		//////////////////////
		
		if (d_config.read_time_tag)
		{
			if ((next_time_tag_index > -1) && (input_sample_index == next_time_tag_offset))
			{
				const gr::tag_t& tag = d_incoming_time_tags[next_time_tag_index];
				
				//d_last_time_seconds = pmt::to_uint64(pmt::tuple_ref(tag.value, 0));
				//d_last_time_fractional_seconds = pmt::to_double(pmt::tuple_ref(tag.value, 1));
				//d_samples_since_last_time_tag = 0;
				
				d_stream_time.seconds = pmt::to_uint64(pmt::tuple_ref(tag.value, 0));
				d_stream_time.fractional_seconds = pmt::to_double(pmt::tuple_ref(tag.value, 1));
				d_stream_time.sample_offset = 0;
fprintf(stderr, "[%s<%i>] updated time\n", name().c_str(), unique_id());
				//time_tag_sample_offset = tag.offset - read_before_this_work;
				
				++next_time_tag_index;
				if (next_time_tag_index == d_incoming_time_tags.size())
					next_time_tag_index = -1;
				else
					next_time_tag_offset = d_incoming_time_tags[next_time_tag_index].offset;
				
				++d_time_tags;
			}
		}
		
		//////////////////////
		
		bool start_burst = false;
		
		if (d_burst_count == 0)	// Do this immediately	// FIXME: Configurable delay
		{
			start_burst = true;
		}
		else // The first burst has occurred
		{
			if (d_config.use_host_time)
			{
				if ((i == 0) && (d_last_burst_system_time_valid) &&	// Only do this on the first iteration (assuming work will complete fast enough)
					(d_in_burst == false))	// This means it will ignore tag-triggered bursts
				{
					d_system_time = boost::get_system_time();
					boost::posix_time::time_duration diff = d_system_time - d_last_burst_system_time;
					//boost::int64_t ticks = diff.ticks();
					//double d = (double)diff.ticks() / (double)d_system_time_ticks_per_second;
					
					double limit = d_config.interval;
					
					if (limit > 0)
					{
						if (d_config.sample_interval)
							limit /= (double)d_config.sample_rate;
						
						if (diff >= boost::posix_time::microseconds(limit * 1e6))
						//if (d >= limit)
						{
//fprintf(stderr, "[%s<%i>] host elapsed %f\n", name().c_str(), unique_id(), ((float)diff.ticks() / (float)d_system_time_ticks_per_second));
							start_burst = true;
						}
					}
				}
			}
			else // Use stream time
			{
				burst_time diff = burst_time_difference(d_stream_time, d_last_burst_time);
				
				double limit = d_config.interval;
				if (d_config.sample_interval == false)
					limit *= (double)d_config.sample_rate;
				
				if (diff.sample_offset >= ((uint64_t)limit))
				{
					start_burst = true;
				}
			}
		}
		
		//////////////////////
		
		if ((start_burst) && (d_in_burst))	// Can only happen if settings are changed, or overlaping trigger tags, or conflict b/w simultaneous interval timing & trigger tag
		{
			// FIXME: Configurable drop current, send now (even if not finished) or finish (ignore current trigger, or immediately queue afterward).
fprintf(stderr, "[%s<%i>] already in burst #%i\n", name().c_str(), unique_id(), d_burst_count);
			d_in_burst = false;	// Start a new one (drop the current)
		}
		
		if ((start_burst) && (d_in_burst == false))
		{
			d_in_burst = true;
			++d_burst_count;
			d_current_burst_length = d_config.burst_length;	// FIXME: Or from tag
//fprintf(stderr, "[%s<%i>] starting burst #%i (length %i)\n", name().c_str(), unique_id(), d_burst_count, d_current_burst_length);
			if (d_config.relative_time == false)	// Implies from the start of the burst
			{
				d_last_burst_system_time = boost::get_system_time();	// This is really meaningless, but store anyway
				d_last_burst_system_time_valid = true;
				d_last_burst_time = d_stream_time;
			}
			
			if ((d_config.output_messages) && (d_config.msgq))
			{
				if (true)	// FIXME: Any other condition that should prevent this?
				{
					//d_msg_ready = false;
					d_burst_message_sample_index = 0;
					// FIXME: Clear any accumulated info (e.g. tags)
				}
			}
		}
		
		//////////////////////
		
		//const int input_tag_port = 0;
		//input_tag_port.clear()
		//get_tags_in_range(tags, input_tag_port, input_sample_index, (input_sample_index + 1));
		
		//////////////////////
		
		if (d_in_burst)
		{
			bool process_msg = (d_config.output_messages) && (d_config.msgq);
			
			if ((process_msg) && (d_burst_message_sample_index < d_current_burst_length))
			{
				memcpy(d_message_buffer + (d_burst_message_sample_index * d_config.item_size), in + (i * d_config.item_size), d_config.item_size);
				
				// FIXME: Remember any tags
			}
			
			if (d_config.output_stream)
			{
				// Propagate tags
				
				//++items_produced;
			}
			
			++d_burst_message_sample_index;
			
			if (d_burst_message_sample_index == d_current_burst_length)
			{
				if (process_msg)
				{
					//d_msg_ready = true;
					
					if (d_pending_msg)
						send_pending_msg();
					
					if ((!d_pending_msg) || (d_config.drop_current))
					{
						if (d_pending_msg)
						{
fprintf(stderr, "."); fflush(stderr);
							// FIXME: Drop counter
						}
						
						int msg_samples_data_length = d_current_burst_length * d_config.item_size;
						int msg_data_length = msg_samples_data_length + 0;	// FIXME: Add additional info
						int flags = 0;
						
						gr::message::sptr msg = gr::message::make(flags, 0, 0, msg_data_length);	// long type, double arg1, double arg2, size_t length
						
						int offset = 0;
						
						// FIXME: Copy header (stream time, trigger mode)
						
						memcpy(msg->msg() + offset, d_message_buffer, msg_samples_data_length);
						offset += msg_samples_data_length;
						
						//memcpy(msg->msg() + offset, , );	// FIXME: Copy additonal info
						//offset += ;
						
						d_pending_msg = msg;
						msg.reset();
					}
					
					//d_current_msg.reset();
				}
				
				if (d_config.output_stream)
				{
					// FIXME: EOB
				}
				
				d_in_burst = false;
			}
		}
		
		//////////////////////
		
		if ((d_config.output_messages) && (d_config.msgq) && (/*d_current_msg*/d_pending_msg)/* && (d_msg_ready)*/)
		{
			send_pending_msg();
		}
		
		//////////////////////
		
		//if (d_samples_since_last_time_tag != -1)
		//	++d_samples_since_last_time_tag;
		++d_stream_time.sample_offset;
	}
	
	////////////////////////////////////
	
	consume(0, input_count);
	
	return items_produced;
}

bool baz_burster::send_pending_msg(void)
{
	bool is_msgq_full = d_config.msgq->full_p();
	if ((is_msgq_full == false)/* || (d_config.drop_current)*/)
	{
		//if (is_msgq_full == false)
		{
			d_config.msgq->/*insert_tail*/handle(d_pending_msg);
			
			if (d_config.relative_time)
			{
				d_last_burst_system_time = boost::get_system_time();
				d_last_burst_system_time_valid = true;
				d_last_burst_time = d_stream_time;	// This is meaningless, but store it anyway
			}
		}
		/*else
		{
			// FIXME: Drop counter
		}*/
		
		d_pending_msg.reset();
		//d_msg_ready = false;
		
		return true;
	}
	
	return false;
}
