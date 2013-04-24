#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  acars_printer.py
#  
#  Copyright 2013 Balint Seeber <balint@crawfish>
#  
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.
#  
#  

"""
	
	enum flags_t
	{
		FLAG_NONE	= 0x00,
		FLAG_SOH	= 0x01,
		FLAG_STX	= 0x02,
		FLAG_ETX	= 0x04,
		FLAG_DLE	= 0x08
	};
	
	struct packet
	{
		float prekey_average;
		int prekey_ones;
		unsigned char bytes[MAX_PACKET_SIZE];
		unsigned char byte_error[MAX_PACKET_SIZE];
		int parity_error_count;
		int byte_count;
		unsigned char flags;
		int etx_index;
	};
"""

_MAX_PACKET_SIZE = 252

#import threading
import gnuradio.gr.gr_threading as _threading
import struct, datetime, time, traceback
from aviation import acars

class acars_struct():
	def __init__(self, data):
		try:
			fmt_str = "=ffi%is%isiiBi" % (_MAX_PACKET_SIZE, _MAX_PACKET_SIZE)	# '=' for #pragma pack(1)
			struct_size = struct.calcsize(fmt_str)
			
			(self.reference_level,
			self.prekey_average,
			self.prekey_ones,
			self.byte_data,
			self.byte_error,
			self.parity_error_count,
			self.byte_count,
			self.flags,
			self.etx_index) = struct.unpack(fmt_str, data[:struct_size])
			
			self.station_name = "".join(data[struct_size:])
			
			#print "Data:", self.byte_data
			#print "Errors:", self.byte_error
		except Exception, e:
			print "Exception unpacking data of length %i: %s" % (len(data), str(e))
			raise e

class queue_watcher_thread(_threading.Thread):
	def __init__(self, msgq, callback=None):
		_threading.Thread.__init__(self)
		self.setDaemon(1)
		self.msgq = msgq
		self.callback = callback
		self.keep_running = True
		self.start()
	def stop(self):
		print "Stopping..."
		self.keep_running = False
	def run(self):
		while self.keep_running:
			msg = self.msgq.delete_head()
			#msg.type() flags
			msg_str = msg.to_string()
			try:
				unpacked = acars_struct(msg_str)
				#print "==> Received ACARS struct with %i pre-key ones" % (unpacked.prekey_ones)
				if self.callback:
					self.callback(unpacked)
				else:
					print "==> Reference level:", unpacked.reference_level	# msg.arg2()
					d = {}
					start = 1	# Skip SOH
					end = unpacked.byte_count-(1+2)	# Skip BCS and DEL
					d['Message'] = unpacked.byte_data[start:end]
					if unpacked.parity_error_count > 0:
						#print "==> Parity error count:", unpacked.parity_error_count
						error_indices = unpacked.byte_error[start:end]
						if '\x01' in error_indices:
							d['ErrorIndices'] = error_indices
							#print map(ord, d['ErrorIndices'])
					d['Time'] = int(time.time() * 1000)
					d['FeedParameters'] = {'StationName':unpacked.station_name, 'Frequency':msg.arg1()/1e6}
					try:
						acars_payload = acars.payload.parse(d)
						print str(acars_payload)
					except Exception, e:
						print "Exception parsing ACARS message: ", e
						traceback.print_exc()
			except Exception, e:
				print "Exception unpacking ACARS message: ", e
				traceback.print_exc()

def main():
	return 0

if __name__ == '__main__':
	main()
