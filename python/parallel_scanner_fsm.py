#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  parallel_scanner_fsm.py
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

import gnuradio.gr.gr_threading as _threading
import struct, datetime, time, traceback, itertools

class fsm(_threading.Thread):
	def __init__(self, msgq, callback=None, sink=None, baseband_freq=0):
		_threading.Thread.__init__(self)
		self.setDaemon(1)
		self.msgq = msgq
		self.callback = callback
		self.sink = sink
		self.baseband_freq = baseband_freq
		self.keep_running = True
		#self.unmuted = False
		self.active_freq = None
		self.start()
	def stop(self):
		print "Stopping..."
		self.keep_running = False
	def run(self):
		while self.keep_running:
			msg = self.msgq.delete_head()
			
			try:
				msg_str = msg.to_string()
				offset = struct.unpack("Q", msg_str[0:8])
				msg_str = msg_str[8:]
				
				params = msg_str.split('\0')
				src_id = None
				key = None
				val = None
				appended = None
				freq_offset = None
				
				try:
					src_id = params[0]
					key = params[1]
					val = params[2]
					appended = params[3]
					freq_offset = float(appended)
				except Exception, e:
					print "Exception while unpacking message:", e
					traceback.print_exc()
				
				#print src_id, key, val, appended
				
				#if val == 'unmuted':
				if key == 'squelch_sob':
					print "[", self.active_freq, "]", src_id, key, val, appended
					colour = (0.,0.,0.)
					if self.active_freq == None:
						print "Unmuted at", self.baseband_freq+freq_offset
						#self.unmuted = True
						self.active_freq = freq_offset
						self.callback(freq_offset)
						colour = (0.0,0.8,0.0)
					if self.sink is not None:
						self.sink.set_line({'id':freq_offset,'type':'v','offset':self.baseband_freq+freq_offset,'action':True,'colour':colour})
				#elif val == 'muted':
				elif key == 'squelch_eob':
					if self.sink is not None:
						self.sink.set_line({'id':freq_offset,'action':False})
					print "[", self.active_freq, "]", src_id, key, val, appended
					if self.active_freq == freq_offset:
						print "Muted."
						self.active_freq = None
			except Exception, e:
					print "Exception while decoding message:", e
					traceback.print_exc()

def main():
	return 0

if __name__ == '__main__':
	main()
