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

#ifndef INCLUDED_BAZ_FASTRAK_DECODER_H
#define INCLUDED_BAZ_FASTRAK_DECODER_H

#include <gnuradio/sync_block.h>

class BAZ_API baz_fastrak_decoder;

/*
 * We use boost::shared_ptr's instead of raw pointers for all access
 * to gr_blocks (and many other data structures).  The shared_ptr gets
 * us transparent reference counting, which greatly simplifies storage
 * management issues.  This is especially helpful in our hybrid
 * C++ / Python system.
 *
 * See http://www.boost.org/libs/smart_ptr/smart_ptr.htm
 *
 * As a convention, the _sptr suffix indicates a boost::shared_ptr
 */
typedef boost::shared_ptr<baz_fastrak_decoder> baz_fastrak_decoder_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_fastrak_decoder.
 *
 * To avoid accidental use of raw pointers, baz_fastrak_decoder's
 * constructor is private.  baz_make_fastrak_decoder is the public
 * interface for creating new instances.
 */
BAZ_API baz_fastrak_decoder_sptr baz_make_fastrak_decoder (int sample_rate);

/*!
 * \brief square2 a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr_sync_block.
 */
class BAZ_API baz_fastrak_decoder : public gr::sync_block
{
private:
	// The friend declaration allows baz_make_fastrak_decoder to access the private constructor.
	friend BAZ_API baz_fastrak_decoder_sptr baz_make_fastrak_decoder (int sample_rate);

	baz_fastrak_decoder (int sample_rate);	// private constructor

	float d_sync_threshold;
	int d_sample_rate;
	int d_oversampling;
	std::string d_last_id_string;
	typedef enum state
	{
		STATE_UNKNOWN,
		STATE_SEARCHING,
		STATE_SYNC,
		STATE_TYPE,
		STATE_DECODE,
		STATE_CRC,
		STATE_DONE
	} state_t;
	typedef enum packet_type
	{
		PT_UNKNOWN		= 0xFFFF,
		PT_ID			= 0x0001
	} packet_type_t;
	packet_type_t d_current_packet_type;
	typedef std::map<packet_type_t,int> TypeLengthMap;
	TypeLengthMap d_type_length_map;
	state_t d_state;
	unsigned long long d_bit_buffer;
	int d_bit_counter;
	int d_sub_symbol_counter;
	int d_payload_bit_counter;
	unsigned int d_id;
	unsigned short d_crc;
	unsigned char d_crc_buffer;
	int d_total_bit_counter;
	bool d_compute_crc;
	int d_crc_bit_counter;
	unsigned int d_last_id;
	unsigned int d_last_id_count;
	
	void enter_state(state_t state);
public:
	~baz_fastrak_decoder ();	// public destructor
	
	void set_sync_threshold(float threshold);
	unsigned int last_id_count(bool reset = false);

	//inline std::string last_id() const
	//{ return d_last_id_string; }
	inline unsigned int last_id() const
	{ return d_last_id; }

	int work (int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_FASTRAK_DECODER_H */
