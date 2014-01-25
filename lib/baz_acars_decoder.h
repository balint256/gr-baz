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

#ifndef INCLUDED_BAZ_ACARS_H
#define INCLUDED_BAZ_ACARS_H

#include <gnuradio/sync_block.h>
#include <gnuradio/msg_queue.h>
#include <string>

class baz_acars_decoder;

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
typedef boost::shared_ptr<baz_acars_decoder> baz_acars_decoder_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_acars_decoder.
 *
 * To avoid accidental use of raw pointers, baz_acars_decoder's
 * constructor is private.  baz_acars_decoder is the public
 * interface for creating new instances.
 */
baz_acars_decoder_sptr baz_make_acars_decoder (gr::msg_queue::sptr msgq);

/*!
 * \brief acars a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr::sync_block.
 */
class baz_acars_decoder : public gr::sync_block
{
private:
	// The friend declaration allows baz_acars_decoder to
	// access the private constructor.
	friend baz_acars_decoder_sptr baz_make_acars_decoder (gr::msg_queue::sptr msgq);

	baz_acars_decoder (gr::msg_queue::sptr msgq);  	// private constructor

	enum state_t
	{
		STATE_SEARCHING,
		//STATE_BITSYNC,
		//STATE_CHARSYNC,
		STATE_ASSEMBLE,
		//STATE_DLE,
		//STATE_END
	};
	
#define MAX_PACKET_SIZE 252
	
	enum flags_t
	{
		FLAG_NONE	= 0x00,
		FLAG_SOH	= 0x01,
		FLAG_STX	= 0x02,
		FLAG_ETX	= 0x04,
		FLAG_DEL	= 0x08
	};

#pragma pack(push)
#pragma pack(1)
	struct packet
	{
		float reference_level;
		float prekey_average;
		int prekey_ones;
		unsigned char byte_data[MAX_PACKET_SIZE];
		unsigned char byte_error[MAX_PACKET_SIZE];
		int parity_error_count;
		int byte_count;
		unsigned char flags;
		int etx_index;
	};
#pragma pack(pop)

	state_t d_state;
	unsigned long d_preamble_state;
	int d_preamble_threshold;
	struct packet d_current_packet;
	int d_bit_counter;
	unsigned char d_current_byte;
	int d_byte_counter;
	unsigned char d_flags;
	gr::msg_queue::sptr d_msgq;
	unsigned char d_prev_bit;
	float d_frequency;
	std::string d_station_name;

public:
	~baz_acars_decoder ();	// public destructor

	void set_preamble_threshold(int threshold);
	void set_frequency(float frequency);
	void set_station_name(const char* station_name);

	inline int preamble_threshold() const
	{ return d_preamble_threshold; }
	inline float frequency() const
	{ return d_frequency; }
	inline const char* station_name() const
	{ return d_station_name.c_str(); }

	int work (int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_ACARS_CC_H */
