/* -*- c++ -*- */
/*
 * Copyright 2007,2008,2009,2010,2013 Free Software Foundation, Inc.
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

#ifndef INCLUDED_BAZ_TCP_SOURCE_H
#define INCLUDED_BAZ_TCP_SOURCE_H

#include <gnuradio/sync_block.h>
#include <gnuradio/thread/thread.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#include <cstddef>
typedef ptrdiff_t ssize_t;
#endif

#include <stdio.h>

class BAZ_API baz_tcp_source;
typedef boost::shared_ptr<baz_tcp_source> baz_tcp_source_sptr;

BAZ_API baz_tcp_source_sptr baz_make_tcp_source(size_t itemsize, const char *host, unsigned short port, int buffer_size=0, bool verbose=false);

/*! 
 * \brief Read stream from an UDP socket.
 * \ingroup source_blk
 *
 * \param itemsize     The size (in bytes) of the item datatype
 * \param host         The name or IP address of the receiving host; can be
 *                     NULL, None, or "0.0.0.0" to allow reading from any
 *                     interface on the host
 * \param port         The port number on which to receive data; use 0 to
 *                     have the system assign an unused port number
 * \param buffer_size  TCP receiver buffer size
 * \param verbose      Output BorIP packet debug messages (helpful to judge packet loss)
 *
*/

class BAZ_API baz_tcp_source : public gr::sync_block
{
private:
	friend BAZ_API baz_tcp_source_sptr baz_make_tcp_source(size_t itemsize, const char *host, unsigned short port, int buffer_size, bool verbose);

	size_t	d_itemsize;
	//bool d_wait;          // wait if data if not immediately available
	int d_socket;        // handle to socket
	char *d_temp_buff;    // hold buffer between calls
	int d_temp_buff_size;
	int d_temp_buff_used;
	//ssize_t d_residual;   // hold information about number of bytes stored in the temp buffer
	int d_temp_offset; // point to temp buffer location offset
	bool d_verbose;
	bool d_eos;
	int d_client_socket;
	//struct sockaddr_in cli_addr;
	char* d_client_addr;
	unsigned int d_client_addr_len;
	int d_packet_type;
	int d_packet_length;
	int d_packet_offset;
	pmt::pmt_t d_tags;
	bool d_new_tags;
	int d_work_count;
	
	void disconnect_client();

protected:
  /*!
   * \brief UDP Source Constructor
   * 
   * \param itemsize     The size (in bytes) of the item datatype
   * \param host         The name or IP address of the receiving host; can be
   *                     NULL, None, or "0.0.0.0" to allow reading from any
   *                     interface on the host
   * \param port         The port number on which to receive data; use 0 to
   *                     have the system assign an unused port number
   * \param buffer_size  TCP receiver buffer size
   * \param verbose      Output BorIP packet debug messages (helpful to judge packet loss)
   */
  baz_tcp_source(size_t itemsize, const char *host, unsigned short port, int buffer_size, bool verbose);

public:
	~baz_tcp_source();

  /*! \brief return the port number of the socket */
	int get_port();

	void signal_eos();

	int work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};


#endif /* INCLUDED_BAZ_TCP_SOURCE_H */
