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
#ifdef IN_GR_BAZ
#include <baz_udp_sink.h>
#else
#include <gnuradio/udp_sink.h>
#endif // IN_GR_BAZ
#include <gnuradio/io_signature.h>
#include <stdexcept>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#if defined(HAVE_NETDB_H)
#include <netdb.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>  //usually included by <netdb.h>?
#endif
typedef void* optval_t;
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
	BYTE flags;
	BYTE notification;
	USHORT idx;
} BOR_PACKET_HEADER, *PBOR_PACKET_HEADER;

typedef struct BorPacket {
	BOR_PACKET_HEADER header;
	BYTE data[1];
} BOR_PACKET, *PBOR_PACKET;

#pragma pack(pop)

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
    fprintf(stderr,"gr_udp_sink/is_error: unknown error %d\n", perr );
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
  return;
}

UDP_SINK_NAME::UDP_SINK_NAME (size_t itemsize, 
			  const char *host, unsigned short port,
			  int payload_size, bool eof, bool bor)
  : gr::sync_block ("udp_sink",
		   gr::io_signature::make (1, 1, itemsize),
		   gr::io_signature::make (0, 0, 0)),
    d_itemsize (itemsize), d_payload_size(0), d_eof(eof),
    d_socket(-1), d_connected(false), d_bor(false),
	d_bor_counter(0), d_bor_first(false), d_bor_packet(NULL), d_residual(0), d_offset(0),
	d_data_length(0)
{
  set_payload_size(payload_size);
  set_borip(bor);
#if defined(USING_WINSOCK) // for Windows (with MinGW)
  // initialize winsock DLL
  WSADATA wsaData;
  int iResult = WSAStartup( MAKEWORD(2,2), &wsaData );
  if( iResult != NO_ERROR ) {
    report_error( "gr_udp_sink WSAStartup", "WSAStartup failed" );
  }
#endif

  create();

  // Get the destination address
  connect(host, port);
}

void UDP_SINK_NAME::set_status_msgq(gr::msg_queue::sptr queue)	// Only call this once before beginning run! (otherwise locking required)
{
  d_status_queue = queue;
}

bool UDP_SINK_NAME::create()
{
  destroy();
  
  // create socket
  d_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (d_socket == -1) {
    report_error("socket open","can't create socket");
	return false;
  }

  // Don't wait when shutting down
  linger lngr;
  lngr.l_onoff  = 1;
  lngr.l_linger = 0;
  if (setsockopt(d_socket, SOL_SOCKET, SO_LINGER, (optval_t)&lngr, sizeof(linger)) == -1) {
    if ( !is_error(ENOPROTOOPT) ) {  // no SO_LINGER for SOCK_DGRAM on Windows
      report_error("SO_LINGER","can't set socket option SO_LINGER");
	  return false;
    }
  }
  
  int requested_send_buff_size = 1024 * 1024;
  if (setsockopt(d_socket, SOL_SOCKET,
#ifdef SO_SNDBUFFORCE
	SO_SNDBUFFORCE
#else
	SO_SNDBUF
#endif // SO_SNDBUFFORCE
	, (optval_t)&requested_send_buff_size, sizeof(int)) == -1) {
	//if (d_verbose) {
	//  fprintf(stderr, "Failed to set receive buffer size: %d\n", requested_send_buff_size);
	//}
  }
  else {
	int send_buff_size = 0;
	socklen_t var_size = 0;
	if ((getsockopt(d_socket, SOL_SOCKET, SO_SNDBUF, (optval_t)&send_buff_size, &var_size) == 0) && (var_size == sizeof(int)) && (send_buff_size != requested_send_buff_size)) {
	  fprintf(stderr, "[UDP Sink \"%s (%ld)\"] Successfully requested %i bytes buffer, but is still %i\n", name().c_str(), unique_id(), requested_send_buff_size, send_buff_size);
	}
  }
  
  //fprintf(stderr, "[UDP Sink \"%s\"] Created socket\n", name().c_str());
  
  return true;
}

void UDP_SINK_NAME::destroy()
{
  if (d_connected)
    disconnect();

  if (d_socket != -1) {
    shutdown(d_socket, SHUT_RDWR);
#if defined(USING_WINSOCK)
    closesocket(d_socket);
#else
    ::close(d_socket);
#endif
    d_socket = -1;
  }
  
  //fprintf(stderr, "[UDP Sink \"%s\"] Destroyed socket\n", name().c_str());
}

// public constructor that returns a shared_ptr

UDP_SINK_SPTR
UDP_SINK_MAKER (size_t itemsize, 
		  const char *host, unsigned short port,
		  int payload_size, bool eof, bool bor)
{
  return gnuradio::get_initial_sptr(new UDP_SINK_NAME (itemsize, 
					    host, port,
					    payload_size, eof, bor));
}

void UDP_SINK_NAME::set_borip(bool enable)
{
  gr::thread::scoped_lock guard(d_mutex);
  
  if (d_bor == enable)
	return;
  
  d_bor = enable;
  
  d_bor_first = true;
  d_bor_counter = 0;
  
  fprintf(stderr, "[UDP Sink \"%s (%ld)\"] BorIP: %s\n", name().c_str(), unique_id(), (enable ? "enabled" : "disabled"));
}

void UDP_SINK_NAME::allocate()
{
  if (d_bor_packet != NULL)
	delete [] d_bor_packet;
  
  d_bor_packet = new unsigned char[offsetof(BOR_PACKET, data) + d_payload_size];
  d_residual = 0;
  d_offset = 0;
}

void UDP_SINK_NAME::set_payload_size(int payload_size)
{
  if (payload_size <= 0)
	return;
  
  gr::thread::scoped_lock guard(d_mutex);
  
  d_payload_size = payload_size;
  
  allocate();
  
  fprintf(stderr, "[UDP Sink \"%s (%ld)\"] Payload size: %d\n", name().c_str(), unique_id(), payload_size);
}

UDP_SINK_NAME::~UDP_SINK_NAME ()
{
  destroy();

#if defined(USING_WINSOCK) // for Windows (with MinGW)
  // free winsock resources
  WSACleanup();
#endif

  if (d_bor_packet != NULL)
	delete [] d_bor_packet;
}

int 
UDP_SINK_NAME::work (int noutput_items,
		   gr_vector_const_void_star &input_items,
		   gr_vector_void_star &output_items)
{
  assert(d_residual >= 0);
  
  const char *in = (const char *) input_items[0];
  /*ssize_t*/int r=0, bytes_sent=0, bytes_to_send=0;
  /*ssize_t*/int total_size = d_residual + noutput_items*d_itemsize;
  int prev_residual = d_residual;
  
  /*if ((d_data_length == 0) || (total_size != d_data_length))
  {
	fprintf(stderr, "[UDP Sink \"%s (%ld)\"] Total size: %i\n", name().c_str(), unique_id(), total_size);
	d_data_length = total_size;
  }*/
  
#if SNK_VERBOSE
  printf("Entered udp_sink\n");
#endif
  
  gr::thread::scoped_lock guard(d_mutex);  // protect d_socket
  
  while (bytes_sent < total_size) {
    bytes_to_send = std::min(/*(ssize_t)*/d_payload_size, (total_size - bytes_sent));
	assert(bytes_to_send > 0);

	if (bytes_to_send < d_payload_size) {
//fprintf(stderr, "[UDP Sink \"%s (%ld)\"] < Total size: %i, To send: %i, Sent: %i, Residual: %i, Prev residual: %i\n", name().c_str(), unique_id(), total_size, bytes_to_send, bytes_sent, d_residual, prev_residual);
	  d_offset = (d_bor ? offsetof(BOR_PACKET, data) : 0);
	  assert(d_bor ? d_offset == 4 : d_offset == 0);
	  //assert((d_residual + bytes_to_send) <= d_payload_size);
	  unsigned char* start = d_bor_packet + d_offset + d_residual;
	  memcpy(start, (in + std::max(0, bytes_sent - prev_residual)), (bytes_to_send - d_residual));
	  d_residual /*+*/= bytes_to_send;
	  assert(d_residual <= d_payload_size);
	  break;
	}
	
	assert(bytes_to_send == d_payload_size);
	
    if (d_connected) {
	  if (d_bor) {
//if (d_residual != 0) fprintf(stderr, "[UDP Sink \"%s (%ld)\"] > Total size: %i, To send: %i, Sent: %i, Residual: %i, Prev residual: %i\n", name().c_str(), unique_id(), total_size, bytes_to_send, bytes_sent, d_residual, prev_residual);
		PBOR_PACKET packet = (PBOR_PACKET)d_bor_packet;
		
		if ((d_residual > 0) && (d_offset != offsetof(BOR_PACKET, data))) {
//fprintf(stderr, "[UDP Sink \"%s (%ld)\"] !> Total size: %i, To send: %i, Sent: %i, Residual: %i, Prev residual: %i\n", name().c_str(), unique_id(), total_size, bytes_to_send, bytes_sent, d_residual, prev_residual);
		  memmove(packet->data, d_bor_packet + d_offset, d_residual);
		  d_offset = offsetof(BOR_PACKET, data);
		}
		
		packet->header.notification = 0;
		packet->header.flags = (d_bor_first ? BF_STREAM_START : 0);
		if (d_status_queue) {
		  while (d_status_queue->empty_p() == false) {
			gr::message::sptr msg = d_status_queue->delete_head();
			fprintf(stderr, "[UDP Sink \"%s (%ld)\"] Received status: 0x%02lx\n", name().c_str(), unique_id(), msg->type());
			packet->header.flags |= msg->type();
		  }
		}
		packet->header.idx = d_bor_counter++;
		//assert((d_residual + (bytes_to_send - d_residual)) == d_payload_size);
		memcpy(packet->data + d_residual, (in + std::max(0, bytes_sent - prev_residual)), (bytes_to_send - d_residual));
		
		r = send(d_socket, (char*)packet, (offsetof(BOR_PACKET, data) + bytes_to_send), 0);
		if (r > 0)
		  r -= offsetof(BOR_PACKET, data);
		
		d_bor_first = false;
	  }
	  else {
		if (d_residual > 0) {
		  if (d_offset != 0) {
//fprintf(stderr, "[UDP Sink \"%s (%ld)\"] !< Total size: %i, To send: %i, Sent: %i, Residual: %i, Prev residual: %i\n", name().c_str(), unique_id(), total_size, bytes_to_send, bytes_sent, d_residual, prev_residual);
			memmove(d_bor_packet, d_bor_packet + d_offset, d_residual);
			d_offset = 0;
		  }
		  
		  memcpy(d_bor_packet + d_residual, (in + std::max(0, bytes_sent - prev_residual)), (bytes_to_send - d_residual));
		  
		  r = send(d_socket, (char*)d_bor_packet, bytes_to_send, 0);
		}
		else
		  r = send(d_socket, (in + std::max(0, bytes_sent - prev_residual)), bytes_to_send, 0);
	  }
	  
      if (r == -1) {         // error on send command
		if ( is_error(ECONNREFUSED) )
		  r = bytes_to_send;  // discard data until receiver is started
		else {
		  report_error("udp_sink",NULL); // there should be no error case where
		  return -1;                  // this function should not exit immediately
		}
      }
    }
    else
      r = bytes_to_send;  // discarded for lack of connection
	
	d_residual = std::max(0, d_residual - r);
    bytes_sent += r;
    
#if SNK_VERBOSE
    printf("\tbyte sent: %d bytes\n", r);
#endif
  }
  
#if SNK_VERBOSE
  printf("Sent: %d bytes (noutput_items: %d)\n", bytes_sent, noutput_items);
#endif

  return noutput_items;
}

void UDP_SINK_NAME::connect( const char *host, unsigned short port )
{
  if (d_connected)
    disconnect();
retry_connect:
  if ((host != NULL) && (host[0] != '\0')) {
    // Get the destination address
    struct addrinfo *ip_dst;
    struct addrinfo hints;
    memset( (void*)&hints, 0, sizeof(hints) );
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    char port_str[12];
    sprintf( port_str, "%d", port );
	
    int ret = getaddrinfo( host, port_str, &hints, &ip_dst );
    if ( ret != 0 ) {
	  freeaddrinfo(ip_dst);
	  ip_dst = NULL;
	  
	  char error_msg[1024];
	  snprintf(error_msg, sizeof(error_msg), "[UDP Sink \"%s (%ld)\"] getaddrinfo(%s:%d) - %s\n", name().c_str(), unique_id(), host, port, gai_strerror(ret));
      report_error(/*"gr_udp_sink/getaddrinfo"*/error_msg,
		/*"can't initialize destination socket"*/error_msg);
	}
	
	bool first = true;
	
    // don't need d_mutex lock when !d_connected
    if (::connect(d_socket, ip_dst->ai_addr, ip_dst->ai_addrlen) == -1) {
	  freeaddrinfo(ip_dst);
	  ip_dst = NULL;
	  
	  if ((errno == EINVAL) && (first)) {
		first = false;
		create();	// Re-create socket for new address
		//fprintf(stderr, "[UDP Sink \"%s\"] Re-trying connection to %s:%d\n", name().c_str(), host, port);
		goto retry_connect;
	  }
      report_error("socket connect", "can't connect to socket");
    }
	
    d_connected = true;
	
	if (ip_dst) {
	  freeaddrinfo(ip_dst);
	  ip_dst = NULL;
	}
	
	fprintf(stderr, "[UDP Sink \"%s (%ld)\"] Connected: %s:%d\n", name().c_str(), unique_id(), host, port);
  }
}

void UDP_SINK_NAME::disconnect()
{
  if (!d_connected)
    return;

  #if SNK_VERBOSE
  printf("gr_udp_sink disconnecting\n");
  #endif

  gr::thread::scoped_lock guard(d_mutex);  // protect d_socket from work()

  if ((d_bor) && (d_bor_first == false)) {
    BOR_PACKET_HEADER end_packet;
    memset(&end_packet, 0x00, sizeof(end_packet));
    end_packet.flags = BF_STREAM_END | BF_EMPTY_PAYLOAD;
    end_packet.idx = d_bor_counter++;
    
    send(d_socket, (char*)&end_packet, sizeof(end_packet), 0);
  }

  // Send a few zero-length packets to signal receiver we are done
  if(d_eof) {
    int i;
    for( i = 0; i < 3; i++ )
      (void) send( d_socket, NULL, 0, 0 );  // ignore errors
  }

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
      report_error("udp_sink/select",NULL);
      #endif
  }
  else if (r > 0) {  // call recv() to get error return
    r = recv(d_socket, (char*)&readfds, sizeof(readfds), 0);
    if(r < 0) {
	#if SNK_VERBOSE
	report_error("udp_sink/recv",NULL);
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

  //fprintf(stderr, "[UDP Sink \"%s\"] Disconnected\n", name().c_str());

  d_connected = false;
}
