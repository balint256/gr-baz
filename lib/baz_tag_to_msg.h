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

#ifndef INCLUDED_BAZ_TAG_TO_MSG_H
#define INCLUDED_BAZ_TAG_TO_MSG_H

#include <gnuradio/sync_block.h>
#include <gnuradio/msg_queue.h>
#include <string>

class baz_tag_to_msg;

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
typedef boost::shared_ptr<baz_tag_to_msg> baz_tag_to_msg_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_tag_to_msg.
 *
 * To avoid accidental use of raw pointers, baz_tag_to_msg's
 * constructor is private.  baz_make_tag_to_msg is the public
 * interface for creating new instances.
 */
baz_tag_to_msg_sptr baz_make_tag_to_msg (int item_size, gr::msg_queue::sptr msgq, const char* append = NULL/*, int initial_buffer_size = -1*/);

/*!
 * \brief square2 a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr::sync_block.
 */
class baz_tag_to_msg : public gr::sync_block
{
private:
	// The friend declaration allows baz_make_tag_to_msg to access the private constructor.

	friend baz_tag_to_msg_sptr baz_make_tag_to_msg (int item_size, gr::msg_queue::sptr msgq, const char* append/*, int initial_buffer_size*/);

	baz_tag_to_msg (int item_size, gr::msg_queue::sptr msgq, const char* append/*, int initial_buffer_size*/);  	// private constructor

	int d_item_size;
	gr::msg_queue::sptr d_msgq;
	//char* d_buffer;
	//int d_buffer_size;
	std::string d_appended;

 public:
	~baz_tag_to_msg ();	// public destructor

	void set_msgq(gr::msg_queue::sptr msgq);
	void set_appended_string(const char* append);

	int work (int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_TAG_TO_MSG_H */
