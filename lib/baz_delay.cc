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

#include <baz_delay.h>
#include <gnuradio/io_signature.h>
#include <string.h>
#include <math.h>

using namespace std;

baz_delay_sptr
baz_make_delay (size_t itemsize, int delay)
{
  return baz_delay_sptr (new baz_delay (itemsize, delay));
}

baz_delay::baz_delay (size_t itemsize, int delay)
  : gr::sync_block ("variable_delay",
		   gr::io_signature::make (1, -1, itemsize),
		   gr::io_signature::make (1, -1, itemsize))
  , d_itemsize(itemsize)
  , d_delay(delay)
  , d_buffer(NULL)
  , d_buffer_length(delay * 2)
  , d_zero(delay)
  , d_buffer_pos(0)
  , d_buffer_use(0)
{
//fprintf(stderr, "Variable delay CTOR: %i\n", delay);	
  if (d_buffer_length > 0)
	d_buffer = (unsigned char*)malloc(d_buffer_length * 2 * itemsize);
  
  //set_delay(delay);
}

void baz_delay::set_delay(int delay)
{
  if (delay < 0)
	return;
  if (delay == d_delay)
	return;
  
  boost::mutex::scoped_lock guard(d_mutex);
  
  if (delay > d_delay)
  {
//fprintf(stderr, "\nLengthening delay: %i\n\n", delay);
	if (delay > d_buffer_length)
	{
//fprintf(stderr, "Reallocing\n");
	  int new_size = std::max(delay * 2, d_buffer_length * 2);	// (delay + 1) & ~1
	  d_buffer = (unsigned char*)realloc(d_buffer, new_size * d_itemsize);
	  int diff = ((d_buffer_pos + d_buffer_use) - d_buffer_length);
	  if (diff > 0)
		memcpy(d_buffer + (d_itemsize * d_buffer_length), d_buffer, (d_itemsize * diff));
	  d_buffer_length = new_size;
	}
	
	int diff = delay - d_delay;
	d_zero += diff;
  }
  else
  {
//fprintf(stderr, "\nShortening delay: %i (Z=%i, BU=%i)\n\n", delay, d_zero, d_buffer_use);
	int diff = d_delay - delay;
	int zero_diff = max(0, d_zero - diff);
	d_zero -= zero_diff;
	diff -= zero_diff;
	//if ((diff) && (d_buffer_use))
	{
	  int buffer_diff = min(d_buffer_use, diff);
	  d_buffer_pos = (d_buffer_pos + buffer_diff) % d_buffer_length;
	  d_buffer_use -= buffer_diff;
	}
  }
  
  d_delay = delay;
}

int
baz_delay::work (int noutput_items,
		gr_vector_const_void_star &input_items,
		gr_vector_void_star &output_items)
{
  assert(input_items.size() == output_items.size());

  boost::mutex::scoped_lock guard(d_mutex);
  
  int zero = min(d_zero, noutput_items);
  int residual = min(zero + d_buffer_use, noutput_items);
  int to_copy_offset = noutput_items - residual;
  int to_copy_length = noutput_items - to_copy_offset;
  assert(to_copy_offset + to_copy_length == noutput_items);
  
  if ((d_buffer_use + to_copy_length) > d_buffer_length)
  {
//fprintf(stderr, "\nReallocing in work\n\n");
	int new_size = max((d_buffer_use + to_copy_length) + (((d_buffer_use + to_copy_length) % 2) ? 1 : 0), d_buffer_length * 2);
	d_buffer = (unsigned char*)realloc(d_buffer, new_size * d_itemsize);
	int diff = ((d_buffer_pos + d_buffer_use) - d_buffer_length);
	if (diff > 0)
	  memcpy(d_buffer + (d_itemsize * d_buffer_length), d_buffer, (d_itemsize * diff));
	d_buffer_length = new_size;
  }
  
  int buffer_free_start = (d_buffer_length ? ((d_buffer_pos + d_buffer_use) % d_buffer_length) : 0);
  int diff = noutput_items - zero;
  
  int to_copy_length1 = to_copy_length;
  int to_copy_length2 = 0;
  if ((d_buffer_length) && (((buffer_free_start + to_copy_length) % d_buffer_length) < buffer_free_start))
  {
	to_copy_length1 = d_buffer_length - buffer_free_start;
	to_copy_length2 = to_copy_length - to_copy_length1;
  }
//fprintf(stderr, "Store in buffer:  %i [%i (%i/%i)]\n", to_copy_offset, to_copy_length, to_copy_length1, to_copy_length2);

  int buf_diff = min(diff, d_buffer_use);
  int i1 = min(d_buffer_length - d_buffer_pos, buf_diff);
  int i2 = buf_diff - i1;
//fprintf(stderr, "Read from buffer: %i [%i (%i/%i)]\n", d_buffer_pos, buf_diff, i1, i2);
  assert(i1 + i2 == buf_diff);

  for (size_t i = 0; i < input_items.size(); i++)	// FIXME: Create multiple buffers for each stream!
  {
    const char* iptr = (const char *) input_items[i];
	char* optr = (char *) output_items[i];
	
	if ((d_delay == 0) ||
		(d_buffer == NULL) || (d_buffer_length == 0))	// Just in case
	{
	  memcpy(optr, iptr, noutput_items * d_itemsize);
	  continue;
	}
//fprintf(stderr, "Work[%i]: N=%i Z=%i D=%i\n", i, noutput_items, zero, diff);
	if (to_copy_length)
	{
	  unsigned char* obuf = d_buffer + (buffer_free_start * d_itemsize);
	  memcpy(obuf, iptr + (d_itemsize * to_copy_offset), (d_itemsize * to_copy_length1));
	  if (to_copy_length2)
		memcpy(d_buffer, iptr + (d_itemsize * (to_copy_offset + to_copy_length1)), (d_itemsize * to_copy_length2));
	  d_buffer_use += to_copy_length;
	}
	
	if (zero)
	{
	  if (d_buffer_use)
	  {
		const unsigned char* z = d_buffer + (d_buffer_pos * d_itemsize);
		for (int i = 0; i < zero; ++i)
		  memcpy(optr + (i * d_itemsize), z, d_itemsize);
	  }
	  else
		memset(optr, 0x00, d_itemsize * zero);
	  optr += d_itemsize * zero;
	}
	if (zero == noutput_items)
	{
//fprintf(stderr, "\nALL ZEROS\n\n");
	  continue;
	}
	
	if (buf_diff)
	{
	  memcpy(optr, d_buffer + (d_buffer_pos * d_itemsize), i1 * d_itemsize);
	  if (i2)
		memcpy(optr + (i1 * d_itemsize), d_buffer, i2 * d_itemsize);
	  optr += buf_diff * d_itemsize;
	  d_buffer_pos = (d_buffer_pos + buf_diff) % d_buffer_length;
	  d_buffer_use -= buf_diff;
	}
	if ((zero + buf_diff) == noutput_items)
	{
//fprintf(stderr, "\nALL ZEROS+BUFFER\n\n");
	  continue;
	}
	int rest = (noutput_items - (zero + buf_diff));
//fprintf(stderr, "I->O: %i\n", rest);
	memcpy(optr, iptr, rest * d_itemsize);
  }
  /*
  if (d_buffer_length)
  {
	d_buffer_pos = (d_buffer_pos + diff) % d_buffer_length;
	d_buffer_use -= diff;
  }
  */
  d_zero -= zero;
//  assert(d_zero >= 0);
  
  return noutput_items;
}
