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

#if (!defined(INCLUDED_GR_UDP_SOURCE_H) && !defined(IN_GR_BAZ)) || (!defined(INCLUDED_BAZ_UDP_SOURCE_H) && defined(IN_GR_BAZ))
#ifdef IN_GR_BAZ
#define INCLUDED_BAZ_UDP_SOURCE_H
#else
#define INCLUDED_GR_UDP_SOURCE_H
#endif // IN_GR_BAZ

#include <gnuradio/sync_block.h>
#include <gnuradio/thread/thread.h>

#ifdef IN_GR_BAZ
#define UDP_SOURCE_NAME   baz_udp_source
#define UDP_SOURCE_MAKER  baz_make_udp_source
#define UDP_SOURCE_SPTR   baz_udp_source_sptr
#else
#define UDP_SOURCE_NAME   gr_udp_source
#define UDP_SOURCE_MAKER  gr_make_udp_source
#define UDP_SOURCE_SPTR   gr_udp_source_sptr
#endif // IN_GR_BAZ

#ifndef _TO_STR
#define _TO_STR(x)        #x
#endif // _TO_STR
#define UDP_SOURCE_STRING _TO_STR(UDP_SOURCE_NAME)

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#include <cstddef>
typedef ptrdiff_t ssize_t;
#endif

#include <stdio.h>

class BAZ_API UDP_SOURCE_NAME;
typedef boost::shared_ptr<UDP_SOURCE_NAME> UDP_SOURCE_SPTR;

BAZ_API UDP_SOURCE_SPTR UDP_SOURCE_MAKER(size_t itemsize, const char *host, 
						unsigned short port,
						int payload_size=1472,
						bool eof=true, bool wait=true, bool bor=false, bool verbose=false);

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
 * \param payload_size UDP payload size by default set to 1472 =
 *                     (1500 MTU - (8 byte UDP header) - (20 byte IP header))
 * \param eof          Interpret zero-length packet as EOF (default: true)
 * \param wait         Wait for data if not immediately available
 *                     (default: true)
 *
*/

class BAZ_API UDP_SOURCE_NAME : public gr::sync_block
{
  friend BAZ_API UDP_SOURCE_SPTR UDP_SOURCE_MAKER(size_t itemsize,
					       const char *host, 
					       unsigned short port,
					       int payload_size,
					       bool eof, bool wait, bool bor, bool verbose);

 private:
  size_t	d_itemsize;
  int           d_payload_size;  // maximum transmission unit (packet length)
  bool          d_eof;           // zero-length packet is EOF
  bool          d_wait;          // wait if data if not immediately available
  int           d_socket;        // handle to socket
  char *d_temp_buff;    // hold buffer between calls
  ssize_t d_residual;   // hold information about number of bytes stored in the temp buffer
  size_t d_temp_offset; // point to temp buffer location offset
  bool			d_bor;
  unsigned short d_bor_counter;
  bool			d_bor_first;
  bool			d_verbose;
  bool			d_eos;

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
   * \param payload_size UDP payload size by default set to 1472 =
   *                     (1500 MTU - (8 byte UDP header) - (20 byte IP header))
   * \param eof          Interpret zero-length packet as EOF (default: true)
   * \param wait         Wait for data if not immediately available
   *                     (default: true)
   * \param bor          Enable BorIP encapsulation
   * \param verbose      Output BorIP packet debug messages (helpful to judge packet loss)
   */
  UDP_SOURCE_NAME(size_t itemsize, const char *host, unsigned short port,
		int payload_size, bool eof, bool wait, bool bor, bool verbose);

 public:
  ~UDP_SOURCE_NAME();

  /*! \brief return the PAYLOAD_SIZE of the socket */
  int payload_size() { return d_payload_size; }

  /*! \brief return the port number of the socket */
  int get_port();
  
  void signal_eos();

  // should we export anything else?

  int work(int noutput_items,
	   gr_vector_const_void_star &input_items,
	   gr_vector_void_star &output_items);
};


#endif /* INCLUDED_GR_UDP_SOURCE_H */
