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

#ifndef INCLUDED_BAZ_TCP_SINK_H
#define INCLUDED_BAZ_TCP_SINK_H

#include <gnuradio/sync_block.h>
#include <gnuradio/thread/thread.h>

#include <gnuradio/sync_block.h>
#include <gnuradio/msg_queue.h>
#include <gnuradio/thread/thread.h>

class BAZ_API baz_tcp_sink;
typedef boost::shared_ptr<baz_tcp_sink> baz_tcp_sink_sptr;

BAZ_API baz_tcp_sink_sptr baz_make_tcp_sink (size_t itemsize, const char *host, unsigned short port, bool blocking = true, bool auto_reconnect = false, bool verbose = false);

/*!
 * \brief Write stream to an TCP socket.
 * \ingroup sink_blk
 * 
 * \param itemsize     The size (in bytes) of the item datatype
 * \param host         The name or IP address of the receiving host; use
 *                     NULL or None for no connection
 * \param port         Destination port to connect to on receiving host
 */

class BAZ_API baz_tcp_sink : public gr::sync_block
{
private:
	friend BAZ_API baz_tcp_sink_sptr baz_make_tcp_sink (size_t itemsize, const char *host, unsigned short port, bool blocking, bool auto_reconnect, bool verbose);
	size_t d_itemsize;

	int d_socket;          // handle to socket
	bool d_connected;       // are we connected?
	gr::thread::mutex d_mutex;           // protects d_socket and d_connected
	gr::msg_queue::sptr d_status_queue;
	bool d_blocking;
	bool d_auto_reconnect;
	bool d_verbose;
	std::string d_last_host;
	unsigned short d_last_port;

protected:
  /*!
   * \brief TCP Sink Constructor
   * 
   * \param itemsize     The size (in bytes) of the item datatype
   * \param host         The name or IP address of the receiving host; use
   *                     NULL or None for no connection
   * \param port         Destination port to connect to on receiving host
   */
	baz_tcp_sink (size_t itemsize, const char *host, unsigned short port, bool blocking, bool auto_reconnect, bool verbose);
  
	bool create();
	void allocate();
	void destroy();
	void _disconnect();

public:
	~baz_tcp_sink ();

  /*! \brief Change the connection to a new destination
   *
   * \param host         The name or IP address of the receiving host; use
   *                     NULL or None to break the connection without closing
   * \param port         Destination port to connect to on receiving host
   *
   * Calls disconnect() to terminate any current connection first.
   */
	bool connect( const char *host, unsigned short port );

  /*! \brief Send zero-length packet (if eof is requested) then stop sending
   *
   * Zero-byte packets can be interpreted as EOF by gr_udp_source.  Note that
   * disconnect occurs automatically when the sink is destroyed, but not when
   * its top_block stops.*/
	void disconnect();
  
	void set_status_msgq(gr::msg_queue::sptr queue);

	int send_data(int type, const char* data, int length);

	int work (int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_TCP_SINK_H */
