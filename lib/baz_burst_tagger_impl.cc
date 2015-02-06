/*
 * Copyright 2014 Balint Seeber <balint256@gmail.com>.
 * Copyright 2013 Bastian Bloessl <bloessl@ccs-labs.org>.
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

#include "baz_burst_tagger_impl.h"

#include <gnuradio/io_signature.h>
#include <boost/format.hpp>
#include <cstdio>

namespace gr {
namespace baz {

burst_tagger_impl::burst_tagger_impl(const std::string& tag_name /*= "length"*/, float mult/* = 1*/, unsigned int pad_front/* = 0*/, unsigned int pad_rear/* = 0*/, bool drop_residue/* = true*/, bool verbose/* = true*/)
		: gr::block("burst_tagger",
			gr::io_signature::make(1, 1, sizeof(gr_complex)),	// FIXME: Custom type
			gr::io_signature::make(1, 1, sizeof(gr_complex)))
		, d_tag_name(pmt::intern(tag_name))
		, d_copy(0)
		, d_mult(mult)
		, d_in_burst(false)
		, d_pad_front(pad_front)
		, d_pad_rear(pad_rear)
		, d_drop_residue(drop_residue)
		, d_to_pad_front(0)
		//, d_to_pad_rear(0)
		, d_current_length(0)
		, d_count(0)
		, d_ignore_name(pmt::intern("ignore"))
		, d_work_count(0)
		, d_verbose(verbose)
{
	if(d_mult <= 0)
		throw std::out_of_range("multiplier must be > 0");
	
	fprintf(stderr, "<%s[%d]> tag name: %s, multiplier: %f, tag front: %d, tag rear: %d, drop residue: %s, verbose: %s\n", name().c_str(), unique_id(), tag_name.c_str(), mult, pad_front, pad_rear, (drop_residue ? "yes" : "no"), (verbose ? "yes" : "no"));
	
	set_relative_rate(1);
	set_tag_propagation_policy(block::TPP_DONT);
}

burst_tagger_impl::~burst_tagger_impl()
{
}

void burst_tagger_impl::add_sob(uint64_t item)
{
	if (d_in_burst)
		fprintf(stderr, "Already in burst!\n");
	static const pmt::pmt_t sob_key = pmt::string_to_symbol("tx_sob");
	static const pmt::pmt_t value = pmt::PMT_T;
	static const pmt::pmt_t srcid = pmt::string_to_symbol(alias());
	add_item_tag(0, item, sob_key, value, srcid);
	d_in_burst = true;
}

void burst_tagger_impl::add_eob(uint64_t item)
{
	if (d_in_burst == false)
		fprintf(stderr, "Not in burst!\n");
	static const pmt::pmt_t eob_key = pmt::string_to_symbol("tx_eob");
	static const pmt::pmt_t value = pmt::PMT_T;
	static const pmt::pmt_t srcid = pmt::string_to_symbol(alias());
	add_item_tag(0, item, eob_key, value, srcid);
	d_in_burst = false;
}

void burst_tagger_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
{
	if ((d_to_pad_front) || ((d_pad_rear) && (d_copy > 0) && (d_copy <= d_pad_rear)))
	{
		ninput_items_required[0] = 0;
	}
	else
	{
		ninput_items_required[0] = noutput_items;
	}
}

int burst_tagger_impl::general_work(int noutput_items, gr_vector_int& ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
	++d_work_count;
	
	/*{
		std::vector<gr::tag_t> all_tags;
		uint64_t nread = nitems_read(0);
		get_tags_in_range(all_tags, 0, nread, nread + noutput_items);
		if (all_tags.empty() == false)
		{
			std::sort(all_tags.begin(), all_tags.end(), tag_t::offset_compare);
			fprintf(stderr, "[%llu] Encountered %lu tags (work started reading at: %llu)\n", d_work_count, all_tags.size(), nread);
			BOOST_FOREACH(tag_t& tag, all_tags)
			{
				std::string key = pmt::symbol_to_string(tag.key);
				fprintf(stderr, "\t%llu: %s\n", tag.offset, key.c_str());
			}
		}
	}*/
	
	const gr_complex *in = (const gr_complex*)input_items[0];
	gr_complex *out = (gr_complex*)output_items[0];

	bool sob = false;

	if (d_copy == 0)
	{
		std::vector<gr::tag_t> tags;
		//std::vector<gr::tag_t> all_tags;
		std::vector<gr::tag_t> ignore_tags;
		const uint64_t nread = nitems_read(0);
		
		get_tags_in_range(tags, 0, nread, nread + noutput_items, d_tag_name);
		std::sort(tags.begin(), tags.end(), tag_t::offset_compare);
		
		//get_tags_in_range(all_tags, 0, nread, nread + noutput_items);
		//std::sort(all_tags.begin(), all_tags.end(), tag_t::offset_compare);
		
		get_tags_in_range(ignore_tags, 0, nread, nread + noutput_items, d_ignore_name);
		std::sort(ignore_tags.begin(), ignore_tags.end(), tag_t::offset_compare);
		
		if (tags.size() > 0)
		{
			tag_t tag = tags.front();
			
			if (tag.offset == nread)
			{
				if (d_in_burst)
				{
					fprintf(stderr, "! Starting burst when already in one!\n");
				}
				else
					++d_count;
				
				double new_len = (double)d_mult * (double)pmt::to_uint64(tag.value);
				d_current_length = (uint64_t)new_len;
				if (new_len != (double)d_current_length)
				{
					//fprintf(stderr, "! Burst #%llu: will miss %f of a sample\n", d_count, (new_len - (double)d_current_length));
				}
				d_copy = d_current_length + d_pad_rear;
				
//				fprintf(stderr, "[%llu] Starting #%llu of %llu items (current length: %llu, tag value: %llu, items read: %llu)\n", d_work_count, d_count, d_copy, d_current_length, pmt::to_uint64(tag.value), nread);
				
				add_sob(nitems_written(0));
				
				sob = true;
				
				BOOST_FOREACH(tag_t& ignore_tag, ignore_tags)
				{
					if (ignore_tag.offset != nread)
					{
						fprintf(stderr, "! Burst #%llu: Ignoring 'ignore' tag at %llu (expecting %llu)\n", d_count, ignore_tag.offset, nread);
						continue;
					}
					
					ignore_tag.offset = nitems_written(0);
					add_item_tag(0, ignore_tag);
					
					//static const pmt::pmt_t ignore_key = pmt::string_to_symbol("ignore");
					//static const pmt::pmt_t value = pmt::PMT_T;
					//static const pmt::pmt_t srcid = pmt::string_to_symbol(alias());
					//add_item_tag(0, nitems_written(0), ignore_key, value, srcid);
					
					break;
				}
				
				if (d_pad_front)
					d_to_pad_front = d_pad_front;
			}
			else	// Copy until the first tag
			{
				BOOST_FOREACH(tag_t& ignore_tag, ignore_tags)
				{
					bool found = false;
					BOOST_FOREACH(tag_t& _tag, tags)
					{
						if (ignore_tag.offset == _tag.offset)
						{
							found = true;
							break;
						}
					}
					
					if (found == false)
					{
						fprintf(stderr, "! Burst #%llu (%s): Bad 'ignore' tag at %llu\n", d_count, (d_in_burst ? "inside" : "outside"), ignore_tag.offset);	//" (expecting %llu for '%s')", tag.offset, pmt::symbol_to_string(tag.key).c_str()
						break;
					}
				}
				
				uint64_t offset = (tag.offset - nread);
				uint64_t cpy = std::min((uint64_t)noutput_items, offset);
				std::memcpy(out, in, cpy * sizeof(gr_complex));
				
				consume(0, cpy);
				
				if (d_in_burst == false)
				{
					if (d_drop_residue)
					{
						if (d_verbose) fprintf(stderr, "[%llu] ! Dropping %llu items outside burst (after #%llu) waiting for tag in %llu items time (noutput_items: %d)\n", d_work_count, cpy, d_count, offset, noutput_items);
						return 0;
					}
					
					if (d_verbose) fprintf(stderr, "Copied %llu items outside burst (after #%llu) waiting for tag\n", cpy, d_count);
				}
				
				return cpy;
			}
		}
		else
		{
			// If no tags, will consume below
		}
	}
	
	assert(d_copy >= 0);
	
	if (sob == false)	// Re-detect SOB after padding front
	{
		if (d_copy == (d_current_length + d_pad_rear))
			sob = true;
	}
	
	if (d_to_pad_front)
	{
		int cpy = std::min((int)d_to_pad_front, noutput_items);
		//fprintf(stderr, "Padding front: %d\n", cpy);
		std::memset(out, 0x00, cpy * sizeof(gr_complex));
		d_to_pad_front -= cpy;
		
		// Nothing consumed
		
		return cpy;	// FIXME: If output buffer space remains, begin copying into that below in same call to work
	}
	
	if (d_copy)
	{
		uint64_t nread = nitems_read(0);
		int copy_start = d_copy;
		
		int cpy = std::min(d_copy, noutput_items);
		
		if ((d_copy > d_pad_rear) && ((d_copy - cpy) < d_pad_rear))
		{
			//fprintf(stderr, ">>> %d,%d - ", d_copy, cpy);
			
			cpy -= (d_pad_rear - (d_copy - cpy));
			
			//fprintf(stderr, "%d\n", cpy);
		}
		
		assert(cpy >= 0);
		
		if (d_copy > d_pad_rear)
		{
			std::vector<gr::tag_t> all_tags;
			uint64_t start = nread + (sob ? 1 : 0);
			get_tags_in_range(all_tags, 0, start, start + cpy - (sob ? 1 : 0));
			if (all_tags.empty() == false)
			{
				std::sort(all_tags.begin(), all_tags.end(), tag_t::offset_compare);
				// FIXME: Polyphase resampler didn't output all expected bits - if length/ignore tags found here, truncate burst.
				fprintf(stderr, "[%llu] ! Encountered %lu tags during burst #%llu (work started reading at: %llu, copying: %d)\n", d_work_count, all_tags.size(), d_count, nread, cpy);
				BOOST_FOREACH(tag_t& tag, all_tags)
				{
					std::string key = pmt::symbol_to_string(tag.key);
					fprintf(stderr, "\t%llu: %s\n", tag.offset, key.c_str());
				}
			}
			
			//fprintf(stderr, "Copying: %d (d_copy = %d)\n", cpy, d_copy);
			
			std::memcpy(out, in, cpy * sizeof(gr_complex));
			
			consume(0, cpy);
		}
		else
		{
			//fprintf(stderr, "Padding rear: %d (d_copy = %d)\n", cpy, d_copy);
			
			std::memset(out, 0x00, cpy * sizeof(gr_complex));
		}
		
		d_copy -= cpy;
		
		assert(d_copy >= 0);
		
		if (d_copy == 0)
		{
			uint64_t eob_time = nitems_written(0) + cpy - 1;
			
//			fprintf(stderr, "[%llu] EOB at %llu (noutput_items: %d, work started reading at: %llu, copied: %d, d_copy was: %d)\n", d_work_count, eob_time, noutput_items, nread, cpy, copy_start);
			
			add_eob(eob_time);
		}
		
		return cpy;
	}
	else
	{
		uint64_t read = nitems_read(0);
		
		if (noutput_items > ninput_items[0])
		{
			fprintf(stderr, "noutput_items: %d > ninput_items: %d\n", noutput_items, ninput_items[0]);
		}
		
		if (d_in_burst == false)
		{
			if (d_drop_residue)
			{
				consume(0, noutput_items);
				fprintf(stderr, "[%llu] ! Dropping %d items outside burst (after #%llu) waiting for tag (work with no tags) (work started reading at: %llu)\n", d_work_count, noutput_items, d_count, read);
				return 0;
			}
			
			if (d_verbose) fprintf(stderr, "Copied %lu items outside burst (after #%llu, work with no tags)\n", noutput_items, d_count);
		}
		
		std::memcpy(out, in, noutput_items * sizeof(gr_complex));
		
		consume(0, noutput_items);
		
		return noutput_items;
	}
}

burst_tagger::sptr burst_tagger::make(const std::string& tag_name /*= "length"*/, float mult/* = 1*/, unsigned int pad_front/* = 0*/, unsigned int pad_rear/* = 0*/, bool drop_residue/* = true*/, bool verbose/*= true*/)
{
	return gnuradio::get_initial_sptr(new burst_tagger_impl(tag_name, mult, pad_front, pad_rear, drop_residue, verbose));
}

} /* namespace baz */
} /* namespace gr */
