/* -*- c++ -*- */
/*
 * Copyright 2007 Free Software Foundation, Inc.
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

#include <baz_burst_buffer.h>
#include <gnuradio/io_signature.h>

#include <string.h>
#include <math.h>
#include <stdio.h>

using namespace std;

baz_burst_buffer_sptr baz_make_burst_buffer (size_t itemsize, int flush_length /*= 0*/, bool verbose /*= false*/)
{
	return baz_burst_buffer_sptr (new baz_burst_buffer (itemsize, flush_length, verbose));
}

baz_burst_buffer::baz_burst_buffer (size_t itemsize, int flush_length /*= 0*/, bool verbose /*= false*/)
  : gr::block ("burst_buffer",
		gr::io_signature::make (1, 1, itemsize),
		gr::io_signature::make (1, 1, itemsize))
	, d_itemsize(itemsize)
	, d_buffer(NULL)
	, d_buffer_size(1024*1024)
	, d_sample_count(0)
	, d_in_burst(false)
	, d_add_sob(false)
	, d_flush_length(flush_length)
	, d_flush_count(0)
	, d_verbose(verbose)
{
	set_tag_propagation_policy(block::TPP_DONT);
	
	fprintf(stderr, "[%s<%i>] item size: %d\n", name().c_str(), unique_id(), itemsize);

	reallocate_buffer();
}

baz_burst_buffer::~baz_burst_buffer()
{
	if (d_buffer)
	{
		free(d_buffer);
		d_buffer = NULL;
	}
}
/*
void baz_delay::set_delay(int delay)	// +ve: past, -ve: future
{
	boost::mutex::scoped_lock guard(d_mutex);

	//fprintf(stderr, "[%s<%i>] delay: %d (was: %d)\n", name().c_str(), unique_id(), delay, d_delay);

	d_delay = delay;
}
*/
void baz_burst_buffer::reallocate_buffer(void)
{
	if (d_buffer == NULL)
	{
		d_buffer = malloc(d_itemsize * d_buffer_size);
	}
	else
	{
		d_buffer_size *= 2;
		
		d_buffer = realloc(d_buffer, d_buffer_size);
	}
	
	assert(d_buffer != NULL);
	
	fprintf(stderr, "[%s<%i>] buffer now: %d samples\n", name().c_str(), unique_id(), d_buffer_size);
}

void baz_burst_buffer::forecast(int noutput_items, gr_vector_int &ninput_items_required)
{
	//boost::mutex::scoped_lock guard(d_mutex);
	
	//const int64_t diff = ((int64_t)nitems_written(0) - (int64_t)nitems_read(0)) - d_delay;
	
	//if (diff != 0)
	//	fprintf(stderr, "[%s<%i>] forecast diff: %d (noutput_items: %d)\n", name().c_str(), unique_id(), diff, noutput_items);
	
	for (size_t i = 0; i < ninput_items_required.size(); ++i)
	{
		//ninput_items_required[i] = ((diff >= 0) ? noutput_items : 0);
		
		if (((d_sample_count > 0) && (d_in_burst == false)) || (d_flush_count > 0))
		{
			ninput_items_required[i] = 0;
		}
		else
		{
			ninput_items_required[i] = noutput_items;
		}
	}
}

static const pmt::pmt_t SOB_KEY = pmt::string_to_symbol("tx_sob");
static const pmt::pmt_t EOB_KEY = pmt::string_to_symbol("tx_eob");
static const pmt::pmt_t IGNORE_KEY = pmt::string_to_symbol("ignore");

int baz_burst_buffer::general_work (int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
	const char *in = (char*)input_items[0];
	char *out = (char*)output_items[0];
	
	//boost::mutex::scoped_lock guard(d_mutex);
	
	const uint64_t nread = nitems_read(0);
	
	if ((d_sample_count > 0) && (d_in_burst == false))
	{
		if (d_verbose)
			fprintf(stderr, "[%s<%i>] Outputting buffer (%d samples)\n", name().c_str(), unique_id(), d_sample_count);
		
		int to_copy = std::min((size_t)noutput_items, d_sample_count);
		
		memcpy(out, d_buffer, d_itemsize * to_copy);
		
		memmove(d_buffer, (char*)d_buffer + (d_itemsize * to_copy), (d_itemsize * (d_sample_count - to_copy)));
		
		if (d_add_sob)
		{
			if (d_verbose) fprintf(stderr, "[%s<%i>] Adding SOB\n", name().c_str(), unique_id());
			
			add_item_tag(0, nitems_written(0), SOB_KEY, pmt::from_bool(true));
			
			d_add_sob = false;
		}
		
		d_sample_count -= to_copy;
		
		if (d_sample_count == 0)
		{
			if (d_verbose) fprintf(stderr, "[%s<%i>] Adding EOB\n", name().c_str(), unique_id());
			
			add_item_tag(0, nitems_written(0)+to_copy-1, EOB_KEY, pmt::from_bool(true));
			
			if (d_flush_length > 0)
				d_flush_count = d_flush_length;
		}
		
		// Not consuming anything
		
		return to_copy;
	}
	else if (d_flush_count > 0)
	{
		int to_go = std::min(noutput_items, d_flush_count);
		
		if (d_flush_count == d_flush_length)
		{
			if (d_verbose) fprintf(stderr, "[%s<%i>] Starting flush at head of work (noutput_items: %d)\n", name().c_str(), unique_id(), noutput_items);
			
			add_item_tag(0, nitems_written(0), SOB_KEY, pmt::from_bool(true));
			add_item_tag(0, nitems_written(0), IGNORE_KEY, pmt::from_bool(true));
		}
		
		memset(out, 0x00, d_itemsize * to_go);
		
		if (to_go == d_flush_count)
		{
			if (d_verbose) fprintf(stderr, "[%s<%i>] Finishing flush in work (noutput_items: %d, to_go: %d)\n", name().c_str(), unique_id(), noutput_items, to_go);
			
			add_item_tag(0, nitems_written(0)+to_go-1, EOB_KEY, pmt::from_bool(true));
		}
		
		d_flush_count -= to_go;
		
		return to_go;
	}
	
	////////////////////////////////////
	
	std::vector<gr::tag_t> tags_sob, tags_eob, tags;
	get_tags_in_range(tags_sob, 0, nread, nread + noutput_items, SOB_KEY);
	get_tags_in_range(tags_eob, 0, nread, nread + noutput_items, EOB_KEY);
	//tags.reserve(tags_sob.size() + tags_eob.size());
	//std::copy(tags_sob.begin(), tags_sob.end(), tags.end());
	//std::copy(tags_eob.begin(), tags_eob.end(), tags.end());
	for (std::vector<gr::tag_t>::iterator it = tags_sob.begin(); it != tags_sob.end(); ++it)
		tags.push_back(*it);
	for (std::vector<gr::tag_t>::iterator it = tags_eob.begin(); it != tags_eob.end(); ++it)
		tags.push_back(*it);
	std::sort(tags.begin(), tags.end(), gr::tag_t::offset_compare);
	
	////////////////////////////////////
	
	int to_copy = 0;
	
	if (tags.empty())
	{
		to_copy = noutput_items;
	}
	else
	{
		gr::tag_t& tag0 = tags[0];
		
		if (tag0.offset != nread)
			to_copy = tag0.offset - nread;
	}
	
	if (to_copy)
	{
		int produced = 0;
		
		if (d_in_burst == false)
		{
			memcpy(out, in, d_itemsize * to_copy);
			
			produced = to_copy;
		}
		else
		{
			while ((d_sample_count + noutput_items) > d_buffer_size)
			{
				reallocate_buffer();
			}
			
			// THIS WILL DROP TAGS INSIDE A BURST!
			
			memcpy((char*)d_buffer + (d_itemsize * d_sample_count), in, d_itemsize * to_copy);
			
			d_sample_count += to_copy;
		}
		
		consume(0, to_copy);
		
		return produced;
	}
	
	////////////////////////////////////
	
	uint64_t burst_length = 0;
	
	BOOST_FOREACH(gr::tag_t& tag, tags)
	{
		if (pmt::equal(tag.key, SOB_KEY))
		{
			if (d_in_burst)
			{
				fprintf(stderr, "[%s<%i>] Already in burst!\n", name().c_str(), unique_id());
			}
			else
			{
				if (d_verbose) fprintf(stderr, "[%s<%i>] Found SOB\n", name().c_str(), unique_id());
				
				assert(nread == tag.offset);	// Should always be first
				
				d_in_burst = true;
			}
		}
		else if (pmt::equal(tag.key, EOB_KEY))
		{
			if (d_in_burst)
			{
				if (d_verbose) fprintf(stderr, "[%s<%i>] Found EOB\n", name().c_str(), unique_id());
				
				burst_length = tag.offset - nread + 1;
				
				d_in_burst = false;
				
				break;
			}
			else
			{
				fprintf(stderr, "[%s<%i>] Not in a burst!\n", name().c_str(), unique_id());
				
				std::string key = pmt::symbol_to_string(tag.key);
				fprintf(stderr, "\t%llu: %s\n", tag.offset, key.c_str());
			}
		}
		else
		{
			fprintf(stderr, "[%s<%i>] Unexpected tag!\n", name().c_str(), unique_id());
			
			assert(false);	// Shouldn't get here
		}
	}
	
	if ((d_in_burst) && (burst_length == 0))
	{
		burst_length = noutput_items;
		
		// Copy everything
	}
	else if ((d_in_burst == false) && (burst_length > 0))
	{
		// Copy up to EOB
	}
	else
	{
		fprintf(stderr, "[%s<%i>] Invalid state!\n", name().c_str(), unique_id());
		
		assert(false);
	}
	
	////////////////////////////////////
	
	d_add_sob = true;
	
	while ((d_sample_count + burst_length) > d_buffer_size)
	{
		reallocate_buffer();
	}
	
	memcpy((char*)d_buffer + (d_itemsize * d_sample_count), in, d_itemsize * burst_length);
	
	d_sample_count += burst_length;
	
	consume(0, burst_length);
	
	return 0;
}
