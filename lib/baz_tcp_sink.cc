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

#include <baz_tcp_sink.h>

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
#include <sys/socket.h>  //usually included by <netdb.h>?
#endif

typedef void* optval_t;

#include <netinet/tcp.h>

#elif defined(HAVE_WINDOWS_H)

// if not posix, assume winsock
#define NOMINMAX
#define USING_WINSOCK
#define snprintf _snprintf
#include <Winsock2.h>
#include <winerror.h>
#include <ws2tcpip.h>
#define SHUT_RDWR 2
typedef char* optval_t;

#endif

#include <gnuradio/thread/thread.h>

#define SNK_VERBOSE 0

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
	BT_TAGS	= 0x02
};

enum BorFlags
{
	BF_NONE				= 0x00,
	BF_HARDWARE_OVERRUN	= 0x01,
	BF_NETWORK_OVERRUN	= 0x02,
	BF_BUFFER_OVERRUN	= 0x04,
	BF_EMPTY_PAYLOAD	= 0x08,
	BF_STREAM_START		= 0x10,
	BF_STREAM_END		= 0x20,
	BF_BUFFER_UNDERRUN	= 0x40,
	BF_HARDWARE_TIMEOUT	= 0x80
};

/////////////////////////////////////////////////

static int is_error( int perr )
{
  // Compare error to posix error code; return nonzero if match.
#if defined(USING_WINSOCK)
#define ENOPROTOOPT 109
#define ECONNREFUSED 111
  // All codes to be checked for must be defined below
  int werr = WSAGetLastError();
  switch( werr ) {
  case WSAETIMEDOUT:
    return( perr == EAGAIN );
  case WSAENOPROTOOPT:
    return( perr == ENOPROTOOPT );
  case WSAECONNREFUSED:
    return( perr == ECONNREFUSED );
  default:
    fprintf(stderr,"baz_tcp_sink/is_error: unknown error %d\n", perr );
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

baz_tcp_sink::baz_tcp_sink (size_t itemsize, const char *host, unsigned short port, bool blocking, bool auto_reconnect, bool verbose)
	: gr::sync_block ("tcp_sink",
		gr::io_signature::make (1, 1, itemsize),
		gr::io_signature::make (0, 0, 0))
	, d_itemsize (itemsize)
	, d_socket(-1)
	, d_connected(false)
	, d_blocking(blocking)
	, d_auto_reconnect(auto_reconnect)
	, d_verbose(verbose)
	, d_last_host(host)
	, d_last_port(port)
{
#if defined(USING_WINSOCK) // for Windows (with MinGW)
	// initialize winsock DLL
	WSADATA wsaData;
	int iResult = WSAStartup( MAKEWORD(2,2), &wsaData );
	if( iResult != NO_ERROR ) {
		report_error( "baz_tcp_sink WSAStartup", "WSAStartup failed" );
	}
#endif

	//create();

	// Get the destination address
	connect(host, port);
}

void baz_tcp_sink::set_status_msgq(gr::msg_queue::sptr queue)	// Only call this once before beginning run! (otherwise locking required)
{
  d_status_queue = queue;
}

bool baz_tcp_sink::create()
{
	destroy();

	// create socket
	d_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (d_socket == -1) {
		report_error("socket open", "can't create socket");
		return false;
	}

	int no_delay = 1;
	if (setsockopt(d_socket, IPPROTO_TCP, TCP_NODELAY, (void *)&no_delay, sizeof(no_delay)) == -1) {
		fprintf(stderr, "[TCP Sink \"%s (%ld)\"] Could not set TCP_NODELAY\n", name().c_str(), unique_id());
	}

	// Don't wait when shutting down
	linger lngr;
	lngr.l_onoff  = 1;
	lngr.l_linger = 0;
	if (setsockopt(d_socket, SOL_SOCKET, SO_LINGER, (optval_t)&lngr, sizeof(linger)) == -1) {
		if ( !is_error(ENOPROTOOPT) ) {  // no SO_LINGER for SOCK_DGRAM on Windows
			report_error("SO_LINGER", "can't set socket option SO_LINGER");
			return false;
		}
	}
/*
	int requested_send_buff_size = 1024 * 1024;
	if (setsockopt(d_socket, SOL_SOCKET,
#ifdef SO_SNDBUFFORCE
		SO_SNDBUFFORCE
#else
		SO_SNDBUF
#endif // SO_SNDBUFFORCE
		, (optval_t)&requested_send_buff_size, sizeof(int)) == -1) {
		//if (d_verbose) {
		//	fprintf(stderr, "Failed to set receive buffer size: %d\n", requested_send_buff_size);
		//}
	}
	else {
		int send_buff_size = 0;
		socklen_t var_size = 0;
		if ((getsockopt(d_socket, SOL_SOCKET, SO_SNDBUF, (optval_t)&send_buff_size, &var_size) == 0) && (var_size == sizeof(int)) && (send_buff_size != requested_send_buff_size)) {
			fprintf(stderr, "[TCP Sink \"%s (%ld)\"] Successfully requested %i bytes buffer, but is still %i\n", name().c_str(), unique_id(), requested_send_buff_size, send_buff_size);
		}
	}
*/
	//fprintf(stderr, "[TCP Sink \"%s\"] Created socket\n", name().c_str());

	return true;
}

void baz_tcp_sink::destroy()
{
	//if (d_connected)
	//	disconnect();

	if (d_socket != -1) {
		shutdown(d_socket, SHUT_RDWR);
#if defined(USING_WINSOCK)
		closesocket(d_socket);
#else
		::close(d_socket);
#endif
		d_socket = -1;
	}

	//fprintf(stderr, "[TCP Sink \"%s\"] Destroyed socket\n", name().c_str());
}

// public constructor that returns a shared_ptr

baz_tcp_sink_sptr baz_make_tcp_sink (size_t itemsize, const char *host, unsigned short port, bool blocking, bool auto_reconnect, bool verbose)
{
	return gnuradio::get_initial_sptr(new baz_tcp_sink (itemsize, host, port, blocking, auto_reconnect, verbose));
}

baz_tcp_sink::~baz_tcp_sink ()
{
	//destroy();
	disconnect();

#if defined(USING_WINSOCK) // for Windows (with MinGW)
	// free winsock resources
	WSACleanup();
#endif
}

int baz_tcp_sink::send_data(int type, const char* data, int length)
{
	BOR_PACKET_HEADER header;
	memset(&header, 0x00, sizeof(header));
	header.type = type;
	header.length = length;
	
	int r;
	
	r = send(d_socket, (char*)&header, sizeof(header), 0);
	if (r < 0) {
		return r;
	}
	
	r = send(d_socket, (char*)data, length, 0);
	if (r < 0) {
		return r;
	}
	
	return length;
}

int baz_tcp_sink::work (int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
	gr::thread::scoped_lock guard(d_mutex);  // protect d_socket

	if (d_connected == false)
	{
		if (d_auto_reconnect == false)
			return -1;
		
		//if (d_verbose)
			fprintf(stderr, "[TCP Sink \"%s (%ld)\"] Attemping re-connect: %s:%d\n", name().c_str(), unique_id(), d_last_host.c_str(), d_last_port);
		
		if (connect(d_last_host.c_str(), d_last_port) == false)
		{
			boost::this_thread::sleep(boost::posix_time::milliseconds(/*d_sleep_duration*/100));	// MAGIC
		
			return (d_blocking ? 0 : noutput_items);	// FIXME: 'connect' will block anyway - was meant for non-blocking re-connect
		}
	}

	////////////////////////////////////////////////////////////////////////////

	const char *in = (const char *)input_items[0];
/*
#if SNK_VERBOSE
	printf("Entered tcp_sink\n");
#endif
*/
	std::vector<gr::tag_t> tags;
	const uint64_t nread = nitems_read(0);
	get_tags_in_range(tags, 0, nread, nread + noutput_items);

	int to_copy = noutput_items;

	if (tags.size() > 0)
	{
		gr::tag_t& tag = tags[0];
		if (tag.offset > nread)
		{
			to_copy = tag.offset - nread;
		}
		else if (tag.offset == nread)
		{
			pmt::pmt_t pdu_meta = pmt::make_dict();
			
			uint64_t next_offset = -1;
			
			BOOST_FOREACH(gr::tag_t& t, tags)
			{
				if (t.offset != nread)
				{
					next_offset = t.offset;
					break;
				}
				
				pdu_meta = dict_add(pdu_meta, t.key, t.value);
			}
			
			/*std::streambuf buf;
			if (pmt::serialize(pdu_meta, buf) == false)
			{
				//
			}*/
			
			std::string tags_str = pmt::serialize_str(pdu_meta);
			
			int r = send_data(BT_TAGS, tags_str.c_str(), tags_str.size() + 1);
			if (r == -1)
			{
				report_error("tcp_sink/tags", NULL);
				
				if (d_verbose) fprintf(stderr, "[TCP Sink \"%s (%ld)\"] Disconnecting...\n", name().c_str(), unique_id());
				
				_disconnect();
				
				return 0;
			}
			
			if (next_offset != -1)
				to_copy = next_offset - nread;
		}
		else
		{
			assert(false);	// Tags before first sample
		}
	}

	int r = send_data(BT_DATA, in, to_copy * d_itemsize);

	if (r == -1)	// error on send command
	{
		/*if (is_error(ECONNREFUSED))
		{
			r = bytes_to_send;	// discard data until receiver is started
		}
		else*/
		{
			report_error("tcp_sink/data", NULL);	// there should be no error case where this function should not exit immediately
			
			_disconnect();
			
			return 0;
		}
	}
/*
#if SNK_VERBOSE
	printf("\tbyte sent: %d bytes\n", r);
#endif

#if SNK_VERBOSE
	printf("Sent: %d bytes (noutput_items: %d)\n", bytes_sent, noutput_items);
#endif
*/
	return to_copy;
}

bool baz_tcp_sink::connect( const char *host, unsigned short port )
{
	if (d_connected)
		disconnect();

	if (create() == false)
		return false;

retry_connect:

	if ((host == NULL) || (host[0] == '\0'))
		return false;
	
	// Get the destination address
	struct addrinfo *ip_dst;
	struct addrinfo hints;
	memset( (void*)&hints, 0, sizeof(hints) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	char port_str[12];
	sprintf( port_str, "%d", port );

	int ret = getaddrinfo( host, port_str, &hints, &ip_dst );
	if ( ret != 0 ) {
		freeaddrinfo(ip_dst);
		ip_dst = NULL;
		
		char error_msg[1024];
		snprintf(error_msg, sizeof(error_msg), "[TCP Sink \"%s (%ld)\"] getaddrinfo(%s:%d) - %s\n", name().c_str(), unique_id(), host, port, gai_strerror(ret));
		report_error(/*"baz_tcp_sink/getaddrinfo"*/error_msg, /*"can't initialize destination socket"*/(d_auto_reconnect ? NULL : error_msg));
		
		return false;	// For 'd_auto_reconnect'
	}

	bool first = true;

	// don't need d_mutex lock when !d_connected
	if (::connect(d_socket, ip_dst->ai_addr, ip_dst->ai_addrlen) == -1) {
		freeaddrinfo(ip_dst);
		ip_dst = NULL;

		if ((errno == EINVAL) && (first)) {
			first = false;
			
			create();	// Re-create socket for new address
			
			//fprintf(stderr, "[TCP Sink \"%s\"] Re-trying connection to %s:%d\n", name().c_str(), host, port);
			
			goto retry_connect;
		}
		
		report_error("socket connect", (d_auto_reconnect ? NULL : "can't connect to socket"));
		
		return false;	// For 'd_auto_reconnect'
	}

	d_connected = true;
	d_last_host = host;
	d_last_port = port;

	if (ip_dst) {
		freeaddrinfo(ip_dst);
		ip_dst = NULL;
	}

	fprintf(stderr, "[TCP Sink \"%s (%ld)\"] Connected: %s:%d\n", name().c_str(), unique_id(), host, port);
	
	return true;
}

void baz_tcp_sink::disconnect()
{
	gr::thread::scoped_lock guard(d_mutex);  // protect d_socket from work()

	_disconnect();
}

void baz_tcp_sink::_disconnect()
{
	if (!d_connected)
		return;

#if SNK_VERBOSE
	printf("baz_tcp_sink disconnecting\n");
#endif

	BOR_PACKET_HEADER end_packet;
	memset(&end_packet, 0x00, sizeof(end_packet));
	end_packet.type = BT_DATA;
	end_packet.flags = BF_STREAM_END | BF_EMPTY_PAYLOAD;

	send(d_socket, (char*)&end_packet, sizeof(end_packet), 0);

	// Sending EOF can produce ERRCONNREFUSED errors that won't show up
	//  until the next send or recv, which might confuse us if it happens
	//  on a new connection.  The following does a nonblocking recv to
	//  clear any such errors.
	timeval timeout;
	timeout.tv_sec = 0;    // zero time for immediate return
	timeout.tv_usec = 0;
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(d_socket, &readfds);
	int r = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
	if(r < 0) {
#if SNK_VERBOSE
		report_error("tcp_sink/select",NULL);
#endif
	}
	else if (r > 0) {  // call recv() to get error return
		r = recv(d_socket, (char*)&readfds, sizeof(readfds), 0);
		if(r < 0) {
#if SNK_VERBOSE
			report_error("tcp_sink/recv",NULL);
#endif
		}
	}

	// Since I can't find any way to disconnect a datagram socket in Cygwin,
	// we just leave it connected but disable sending.
#if 0
	// zeroed address structure should reset connection
	struct sockaddr addr;
	memset( (void*)&addr, 0, sizeof(addr) );
	// addr.sa_family = AF_UNSPEC;  // doesn't work on Cygwin
	// addr.sa_family = AF_INET;  // doesn't work on Cygwin

	if(::connect(d_socket, &addr, sizeof(addr)) == -1)
		report_error("socket connect","can't connect to no socket");
#endif

	//fprintf(stderr, "[TCP Sink \"%s\"] Disconnected\n", name().c_str());

	d_connected = false;
	
	destroy();
}
