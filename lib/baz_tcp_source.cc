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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <baz_tcp_source.h>

#include <gnuradio/io_signature.h>

#include <stdexcept>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#if defined(HAVE_NETDB_H)
#include <netdb.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

typedef void* optval_t;

// ntohs() on FreeBSD may require both netinet/in.h and arpa/inet.h, in order
#if defined(HAVE_NETINET_IN_H)
#include <netinet/in.h>
#endif

#if defined(HAVE_ARPA_INET_H)
#include <arpa/inet.h>
#endif

#elif defined(HAVE_WINDOWS_H)

// if not posix, assume winsock
#define USING_WINSOCK
#define NOMINMAX
#include <Winsock2.h>
#include <winerror.h>
#include <ws2tcpip.h>
#define SHUT_RDWR 2
typedef char* optval_t;

#endif

/////////////////////////////////////////////////

#pragma pack(push)
#pragma pack(1)

typedef unsigned char BYTE;
typedef unsigned short USHORT;

typedef struct BorPacketHeader {
	BYTE type;
	BYTE flags;
	//BYTE notification;
	//USHORT idx;
	uint32_t length;
} BOR_PACKET_HEADER, *PBOR_PACKET_HEADER;

#pragma pack(pop)

enum BorType
{
	BT_NONE	= 0x00,
	BT_DATA	= 0x01,
	BT_TAGS	= 0x02,
	BT_MAX,
	BT_VALID = ((BT_MAX-1) << 1) - 1
};

enum BorFlags
{
	BF_NONE				= 0x00,
	BF_HARDWARE_OVERRUN	= 0x01,
	BF_NETWORK_OVERRUN	= 0x02,
	BF_BUFFER_OVERRUN	= 0x04,
	BF_EMPTY_PAYLOAD	= 0x08,
	BF_STREAM_START		= 0x10,
	BF_STREAM_END		= 0x20
};

/////////////////////////////////////////////////

#define DEFAULT_BUFFER_SIZE	(1024*1024)

#define USE_SELECT		1  // non-blocking receive on all platforms
#define USE_RCV_TIMEO	0  // non-blocking receive on all but Cygwin
#define SRC_VERBOSE		0

static int is_error( int perr )
{
	// Compare error to posix error code; return nonzero if match.
#if defined(USING_WINSOCK)
#ifndef ENOPROTOOPT
#define ENOPROTOOPT 109
#endif
	// All codes to be checked for must be defined below
	int werr = WSAGetLastError();
	switch( werr ) {
		case WSAETIMEDOUT:
			return( perr == EAGAIN );
		case WSAENOPROTOOPT:
			return( perr == ENOPROTOOPT );
		default:
			fprintf(stderr, "baz_tcp_source/is_error: unknown error %d\n", perr );
			throw std::runtime_error("internal error");
	}
	return 0;
#else
	return( perr == errno );
#endif
}

static void report_error( const char *msg1, const char *msg2 )
{
	// Deal with errors, both posix and winsock
#if defined(USING_WINSOCK)
	int werr = WSAGetLastError();
	fprintf(stderr, "%s: winsock error %d\n", msg1, werr );
#else
	perror(msg1);
#endif
	if( msg2 != NULL )
		throw std::runtime_error(msg2);
}

baz_tcp_source::baz_tcp_source(size_t itemsize, const char *host, unsigned short port, int buffer_size, bool verbose)
	: gr::sync_block ("tcp_source",
		gr::io_signature::make(0, 0, 0),
		gr::io_signature::make(1, 1, itemsize))
	, d_itemsize(itemsize)
	, d_socket(-1)
	//, d_residual(0)
	, d_temp_offset(0)
	, d_eos(false)
	, d_temp_buff(NULL)
	, d_temp_buff_size(0)
	, d_temp_buff_used(0)
	, d_client_socket(-1)
	, d_client_addr(NULL)
	, d_client_addr_len(0)
	, d_verbose(verbose)
	, d_packet_type(BT_NONE)
	, d_packet_length(0)
	, d_packet_offset(0)
	, d_tags(pmt::PMT_NIL)
	, d_new_tags(false)
	, d_work_count(0)
{
	if (buffer_size <= 0)
		buffer_size = DEFAULT_BUFFER_SIZE;
	
	fprintf(stderr, "[%s<%i>] item size: %d, host: %s, port: %hu, buffer size: %d, verbose: %s\n", name().c_str(), unique_id(), itemsize, host, port, buffer_size, (verbose ? "yes" : "no"));
	
	int ret = 0;

#if defined(USING_WINSOCK) // for Windows (with MinGW)
	// initialize winsock DLL
	WSADATA wsaData;
	int iResult = WSAStartup( MAKEWORD(2,2), &wsaData );
	if( iResult != NO_ERROR ) {
		report_error( "baz_tcp_source/WSAStartup", "can't open socket" );
	}
#endif

	// Set up the address stucture for the source address and port numbers
	// Get the source IP address from the host name
	struct addrinfo *ip_src = NULL;      // store the source IP address to use
	struct addrinfo hints;
	memset( (void*)&hints, 0, sizeof(hints) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	char port_str[12];
	sprintf( port_str, "%d", port );

	ret = getaddrinfo( host, port_str, &hints, &ip_src );
	if( ret != 0 )
	{
		if (ip_src != NULL)
			freeaddrinfo(ip_src);
		report_error("baz_tcp_source/getaddrinfo", "can't initialize source socket");
	}
	
	d_client_addr_len = ip_src->ai_addrlen;

	// create socket
	d_socket = socket(ip_src->ai_family, ip_src->ai_socktype, ip_src->ai_protocol);
	if (d_socket == -1)
	{
		freeaddrinfo(ip_src);
		report_error("socket open", "can't open socket");
	}

	// Turn on reuse address
	int opt_val = 1;
	if (setsockopt(d_socket, SOL_SOCKET, SO_REUSEADDR, (optval_t)&opt_val, sizeof(int)) == -1)
	{
		freeaddrinfo(ip_src);
		report_error("SO_REUSEADDR", "can't set socket option SO_REUSEADDR");
	}
/*
	// Don't wait when shutting down
	linger lngr;
	lngr.l_onoff  = 1;
	lngr.l_linger = 0;
	if (setsockopt(d_socket, SOL_SOCKET, SO_LINGER, (optval_t)&lngr, sizeof(linger)) == -1)
	{
		if( !is_error(ENOPROTOOPT) ) {  // no SO_LINGER for SOCK_DGRAM on Windows
			freeaddrinfo(ip_src);
			report_error("SO_LINGER", "can't set socket option SO_LINGER");
		}
	}
*/
#if USE_RCV_TIMEO
	// Set a timeout on the receive function to not block indefinitely
	// This value can (and probably should) be changed
	// Ignored on Cygwin
#if defined(USING_WINSOCK)
	DWORD timeout = 1000;  // milliseconds
#else
	timeval timeout;
	timeout.tv_sec = 1;	// MAGIC
	timeout.tv_usec = 0;
#endif
	if(setsockopt(d_socket, SOL_SOCKET, SO_RCVTIMEO, (optval_t)&timeout, sizeof(timeout)) == -1)
	{
		freeaddrinfo(ip_src);
		report_error("SO_RCVTIMEO", "can't set socket option SO_RCVTIMEO");
	}
#endif // USE_RCV_TIMEO
/*
	int requested_recv_buff_size = 1024 * 1024;
	if (setsockopt(d_socket, SOL_SOCKET,
#ifdef SO_RCVBUFFORCE
	SO_RCVBUFFORCE
#else
	SO_RCVBUF
#endif // SO_RCVBUFFORCE
	, (optval_t)&requested_recv_buff_size, sizeof(int)) == -1) {
		if (d_verbose) {
			fprintf(stderr, "Failed to set receive buffer size: %d\n", requested_recv_buff_size);
		}
	}
	else {
		int recv_buff_size = 0;
		socklen_t var_size = 0;
		if ((getsockopt(d_socket, SOL_SOCKET, SO_RCVBUF, (optval_t)&recv_buff_size, &var_size) == 0) && (var_size == sizeof(int)) && (recv_buff_size != requested_recv_buff_size)) {
			fprintf(stderr, "BorUDP Source: successfully requested %i bytes buffer, but is still %i\n", requested_recv_buff_size, recv_buff_size);
		}
	}
*/
	// bind socket to an address and port number to listen on
	if(bind(d_socket, ip_src->ai_addr, ip_src->ai_addrlen) == -1)
	{
		freeaddrinfo(ip_src);
		report_error("socket bind", "can't bind socket");
	}

	freeaddrinfo(ip_src);
	
	if (listen(d_socket, 1) < 0)
	{
		report_error("socket listen", "cannot listen");
	}

	d_temp_buff = new char[buffer_size];   // allow it to hold up to payload_size bytes
	d_temp_buff_size = buffer_size;
	
	d_client_addr = new char[d_client_addr_len];
}

baz_tcp_source_sptr baz_make_tcp_source (size_t itemsize, const char *ipaddr, unsigned short port, int buffer_size, bool verbose)
{
	return gnuradio::get_initial_sptr(new baz_tcp_source (itemsize, ipaddr, port, buffer_size, verbose));
}

baz_tcp_source::~baz_tcp_source ()
{
	disconnect_client();

	// Server
	if (d_socket != -1)
	{
		shutdown(d_socket, SHUT_RDWR);
#if defined(USING_WINSOCK)
		closesocket(d_socket);
#else
		::close(d_socket);
#endif
		d_socket = -1;
	}

#if defined(USING_WINSOCK) // for Windows (with MinGW)
	// free winsock resources
	WSACleanup();
#endif

	if (d_temp_buff)
		delete [] d_temp_buff;
	
	if (d_client_addr)
		delete [] d_client_addr;
}

void baz_tcp_source::disconnect_client()
{
	// Connected client
	if (d_client_socket != -1)
	{
		shutdown(d_client_socket, SHUT_RDWR);
#if defined(USING_WINSOCK)
		closesocket(d_client_socket);
#else
		::close(d_client_socket);
#endif
		d_client_socket = -1;
	}
	
	d_packet_type = BT_NONE;
	d_packet_length = 0;
	d_temp_buff_used = 0;
	d_temp_offset = 0;
}

void baz_tcp_source::signal_eos()
{
	//d_wait = false;
	d_eos = true;
}

int baz_tcp_source::work (int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
	++d_work_count;
	
	if (d_eos)
		return -1;

	////////////////////////////////////////////////////////////////////////////

	char *out = (char*)output_items[0];
	
	int data_remaining_in_buffer = d_temp_buff_used - d_temp_offset;

	if ((d_packet_type == BT_DATA) && (d_packet_length > 0))
	{
		if (data_remaining_in_buffer > 0)
		{
			int remaining_in_packet = d_packet_length - d_packet_offset;
			int remaining = std::min(remaining_in_packet, data_remaining_in_buffer);
			int remaining_items = remaining / d_itemsize;
			int to_copy = std::min(noutput_items, remaining_items);
			int to_copy_bytes = to_copy * d_itemsize;
			
			if (to_copy_bytes > 0)
			{
				if (d_new_tags)
				{
					if (pmt::eq(d_tags, pmt::PMT_NIL) == false)
					{
						pmt::pmt_t klist(pmt::dict_keys(d_tags));
						
						for(size_t i = 0; i < pmt::length(klist); i++)
						{
							pmt::pmt_t k(pmt::nth(i, klist));
							pmt::pmt_t v(pmt::dict_ref(d_tags, k, pmt::PMT_NIL));
							add_item_tag(0, nitems_written(0), k, v, pmt::mp(alias()));
						}
					}
					
					d_new_tags = false;
				}
				
				memcpy(out, d_temp_buff + d_temp_offset, to_copy_bytes);
				
				int residual = data_remaining_in_buffer - to_copy_bytes;
//fprintf(stderr, "[%s<%i>] work %d, in data (temp buff usage: %d) copied bytes: %d, packet length: %d, residual: %d\n", name().c_str(), unique_id(), d_work_count, d_temp_buff_used, to_copy_bytes, d_packet_length, residual);
				// FIXME: Item size mismatch
				//	Careful with next 'assert'
				//if ((remaining_in_packet < d_itemsize) && (data_remaining_in_buffer > 
				
				if (residual > 0)
					memmove(d_temp_buff, d_temp_buff + d_temp_offset + to_copy_bytes, residual);
				
				d_temp_buff_used -= (d_temp_offset + to_copy_bytes);
				assert(d_temp_buff_used == residual);
				d_temp_offset = 0;
				
				d_packet_offset += to_copy_bytes;
				
				if (d_packet_offset == d_packet_length)
				{
					d_packet_type = BT_NONE;
					d_packet_offset = 0;
					d_packet_length = 0;
				}
				
				return to_copy;
			}
			else
			{
				// Need to receive more data
			}
		}
		else
		{
			// Need to receive more data
		}
	}

	////////////////////////////////////////////////////////////////////////////

	if (d_client_socket == -1)
	{
#if USE_SELECT
		// RCV_TIMEO doesn't work on all systems (e.g., Cygwin)
		// use select() instead of, or in addition to RCV_TIMEO
		fd_set readfds;
		
		timeval timeout;
		timeout.tv_sec = 0;			// Init timeout each iteration.  Select can modify it.
		timeout.tv_usec = 1000*10;	// MAGIC: 10 ms
		
		FD_ZERO(&readfds);
		FD_SET(d_socket, &readfds);
		
		int r = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
		
		if (r < 0)
		{
			report_error("tcp_source/select", NULL);
			return -1;
		}
		else if (r == 0)	// timed out
		{
			boost::this_thread::interruption_point();	// Allow boost thread interrupt, then try again
			return 0;	// Was 'continue'
		}
		
		d_client_socket = accept(d_socket, (struct sockaddr *)d_client_addr, &d_client_addr_len);
		if (d_client_socket < 0)
		{
			report_error("tcp_source/accept", NULL);
			return 0;
		}
		
		fprintf(stderr, "[%s<%i>] accepted connection (socket: %d)\n", name().c_str(), unique_id(), d_client_socket);
#endif // USE_SELECT
	}

	////////////////////////////////////////////////////////////////////////////

#if SRC_VERBOSE
	printf("\nEntered tcp_source\n");
#endif

	// Can only get here if there are NO samples to be copied
	int free_buffer_space = d_temp_buff_size - d_temp_buff_used;
	if (free_buffer_space > 0)
	{
//fprintf(stderr, "."); fflush(stderr);
#if USE_SELECT
		// RCV_TIMEO doesn't work on all systems (e.g., Cygwin)
		// use select() instead of, or in addition to RCV_TIMEO
		fd_set readfds;
	
		timeval timeout;
		timeout.tv_sec = 0;			// Init timeout each iteration.  Select can modify it.
		timeout.tv_usec = 1000*10;	// 10 ms
	
		FD_ZERO(&readfds);
		FD_SET(d_client_socket, &readfds);
	
		int r = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
		if (r < 0)
		{
			report_error("tcp_source/select", NULL);
			//return -1;
			
			disconnect_client();
			
			return 0;
		}
		else if (r == 0)	// timed out
		{
//fprintf(stderr, "_"); fflush(stderr);
			boost::this_thread::interruption_point();	// Allow boost thread interrupt, then try again
			
			// Free buffer space, but no data yet available
			// Might be:
			//	- in a data packet
			//	- in a tag packet (assumption is it'll fit in the temp buffer)
			//	- parsing a new packet header
//fprintf(stderr, "[%s<%i>] timed out (%d bytes remaining in buffer)\n", name().c_str(), unique_id(), data_remaining_in_buffer);
			if (data_remaining_in_buffer == 0)
				return 0;
		}
		
		r = recv(d_client_socket, d_temp_buff + d_temp_buff_used, free_buffer_space, 0);
		if (r < 0)
		{
			report_error("tcp_source/recv", NULL);
			//return -1;
			
			disconnect_client();
			
			return 0;
		}
		else if (r == 0)
		{
//fprintf(stderr, "!"); fflush(stderr);
			fprintf(stderr, "[%s<%i>] recv returned 0 - disconnecting client\n", name().c_str(), unique_id());
			
			disconnect_client();
			
			return 0;
		}
		
//fprintf(stderr, "[%s<%i>] received %d bytes\n", name().c_str(), unique_id(), r);
		
		d_temp_buff_used += r;
	}
#endif // USE_SELECT

	while ((d_temp_buff_used - d_temp_offset) > 0)
	{
//fprintf(stderr, "[%s<%i>] work %d, in parse loop (temp buff usage: %d) packet type: %d\n", name().c_str(), unique_id(), d_work_count, d_temp_buff_used, d_packet_type);
		if (d_packet_type == BT_NONE)
		{
			assert(d_temp_offset == 0);
			
			if (d_temp_buff_used < sizeof(BOR_PACKET_HEADER))
				return 0;
			
			PBOR_PACKET_HEADER pHeader = (PBOR_PACKET_HEADER)d_temp_buff;
			
			d_packet_type = pHeader->type;
			assert(d_packet_type & BT_VALID);
			
			d_packet_length = pHeader->length;
			assert(d_packet_length >= 0);
			
			if (d_packet_length == 0)
			{
				assert(pHeader->flags & BF_EMPTY_PAYLOAD);
				d_packet_type = BT_NONE;
			}
			else
			{
				assert((pHeader->flags & BF_EMPTY_PAYLOAD) != BF_EMPTY_PAYLOAD);
			}
			
			d_temp_buff_used -= sizeof(BOR_PACKET_HEADER);
			if (d_temp_buff_used > 0)
				memmove(d_temp_buff, d_temp_buff + sizeof(BOR_PACKET_HEADER), d_temp_buff_used);
			//d_temp_offset is 0
		}
		else if (d_packet_type == BT_DATA)
		{
			assert(d_packet_length > 0);
			
			return 0;	// Process data up top
		}
		else if (d_packet_type == BT_TAGS)
		{
			assert(d_temp_offset == 0);
			
			assert(d_packet_length <= d_temp_buff_size);	// FIXME: Gracefully skip
			
			data_remaining_in_buffer = d_temp_buff_used - d_temp_offset;
			
			if (data_remaining_in_buffer < d_packet_length)
				return 0;
			
			std::string tag_str((const char*)(d_temp_buff + d_temp_offset), d_packet_length);
			d_tags = pmt::deserialize_str(tag_str);
			d_new_tags = true;
			
			d_temp_buff_used -= d_packet_length;
			if (d_temp_buff_used > 0)
				memmove(d_temp_buff, d_temp_buff + d_packet_length, d_temp_buff_used);
			//d_temp_offset is 0
			
			d_packet_type = BT_NONE;
		}
		else
		{
			assert(false);
		}
	}

	return (d_eos ? -1 : 0);
}

// Return port number of d_socket
int baz_tcp_source::get_port(void)
{
	sockaddr_in name;
	socklen_t len = sizeof(name);
	
	int ret = getsockname( d_socket, (sockaddr*)&name, &len );
	if( ret )
	{
		report_error("baz_tcp_source/getsockname",NULL);
		return -1;
	}
	
	return ntohs(name.sin_port);
}
