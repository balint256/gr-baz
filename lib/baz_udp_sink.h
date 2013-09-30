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

#if (!defined(INCLUDED_GR_UDP_SINK_H) && !defined(IN_GR_BAZ)) || (!defined(INCLUDED_BAZ_UDP_SINK_H) && defined(IN_GR_BAZ))
#ifdef IN_GR_BAZ
#define INCLUDED_BAZ_UDP_SINK_H
#else
#define INCLUDED_GR_UDP_SINK_H
#endif // IN_GR_BAZ

#include <gnuradio/sync_block.h>
#include <gnuradio/thread/thread.h>

#ifdef IN_GR_BAZ
#define UDP_SINK_NAME   baz_udp_sink
#define UDP_SINK_MAKER  baz_make_udp_sink
#define UDP_SINK_SPTR   baz_udp_sink_sptr
#else
#define UDP_SINK_NAME   gr_udp_sink
#define UDP_SINK_MAKER  gr_make_udp_sink
#define UDP_SINK_SPTR   gr_udp_sink_sptr
#endif // IN_GR_BAZ

#ifndef _TO_STR
#define _TO_STR(x)        #x
#endif // _TO_STR
#define UDP_SINK_STRING _TO_STR(UDP_SINK_NAME)

#include <gnuradio/sync_block.h>
#include <gnuradio/msg_queue.h>
#include <gnuradio/thread/thread.h>

class BAZ_API UDP_SINK_NAME;
typedef boost::shared_ptr<UDP_SINK_NAME> UDP_SINK_SPTR;

BAZ_API UDP_SINK_SPTR
UDP_SINK_MAKER (size_t itemsize, 
		  const char *host, unsigned short port,
		  int payload_size=1472, bool eof=true, bool bor=false);

/*!
 * \brief Write stream to an UDP socket.
 * \ingroup sink_blk
 * 
 * \param itemsize     The size (in bytes) of the item datatype
 * \param host         The name or IP address of the receiving host; use
 *                     NULL or None for no connection
 * \param port         Destination port to connect to on receiving host
 * \param payload_size UDP payload size by default set to 1472 =
 *                     (1500 MTU - (8 byte UDP header) - (20 byte IP header))
 * \param eof          Send zero-length packet on disconnect
 */

class BAZ_API UDP_SINK_NAME : public gr::sync_block
{
  friend BAZ_API UDP_SINK_SPTR UDP_SINK_MAKER (size_t itemsize, 
					    const char *host,
					    unsigned short port,
					    int payload_size, bool eof, bool bor);
 private:
  size_t	d_itemsize;

  int           d_payload_size;    // maximum transmission unit (packet length)
  bool          d_eof;             // send zero-length packet on disconnect
  int           d_socket;          // handle to socket
  bool          d_connected;       // are we connected?
  gr::thread::mutex  d_mutex;           // protects d_socket and d_connected
  bool          d_bor;
  unsigned short d_bor_counter;
  bool          d_bor_first;
  unsigned char* d_bor_packet;
  int           d_residual;
  int           d_offset;
  int           d_data_length;
  gr::msg_queue::sptr d_status_queue;

 protected:
  /*!
   * \brief UDP Sink Constructor
   * 
   * \param itemsize     The size (in bytes) of the item datatype
   * \param host         The name or IP address of the receiving host; use
   *                     NULL or None for no connection
   * \param port         Destination port to connect to on receiving host
   * \param payload_size UDP payload size by default set to 
   *                     1472 = (1500 MTU - (8 byte UDP header) - (20 byte IP header))
   * \param eof          Send zero-length packet on disconnect
   * \param bor          Enable BorIP encapsulation
   */
  UDP_SINK_NAME (size_t itemsize, 
	       const char *host, unsigned short port,
	       int payload_size, bool eof, bool bor);
  
  bool create();
  void allocate();
  void destroy();

 public:
  ~UDP_SINK_NAME ();

  /*! \brief return the PAYLOAD_SIZE of the socket */
  int payload_size() { return d_payload_size; }

  /*! \brief Change the connection to a new destination
   *
   * \param host         The name or IP address of the receiving host; use
   *                     NULL or None to break the connection without closing
   * \param port         Destination port to connect to on receiving host
   *
   * Calls disconnect() to terminate any current connection first.
   */
  void connect( const char *host, unsigned short port );

  /*! \brief Send zero-length packet (if eof is requested) then stop sending
   *
   * Zero-byte packets can be interpreted as EOF by gr_udp_source.  Note that
   * disconnect occurs automatically when the sink is destroyed, but not when
   * its top_block stops.*/
  void disconnect();
  
  void set_borip(bool enable);
  void set_payload_size(int payload_size);
  void set_status_msgq(gr::msg_queue::sptr queue);

  int work (int noutput_items,
	    gr_vector_const_void_star &input_items,
	    gr_vector_void_star &output_items);
};

#endif /* INCLUDED_GR_UDP_SINK_H */
