#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  freq_hopper.py
#  
#  Copyright 2014 Balint Seeber <balint256@gmail.com>
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

import threading, time, traceback, sys

from gnuradio import gr

class hopper_thread(threading.Thread):
	def __init__(self, u, delay, freqs, auto_start=True, hop=True, **kwds):
		threading.Thread.__init__(self, **kwds)
		self.setDaemon(True)
		self.u = u
		self.delay = delay
		self.freqs = freqs
		self.hop = hop
		self.last_time = None
		self.idx = -1
		self.stop_event = threading.Event()
		self.keep_running = False
		if auto_start:
			print "Auto start"
			self.start()
	def run(self):
		self.keep_running = True
		print "Hopper running with %d freqs" % (len(self.freqs))
		while self.keep_running:
			if self.keep_running == False:
				break
			try:
				if self.hop:
					self.idx += 1
					if self.idx == len(self.freqs):
						self.idx = 0
					freq = self.freqs[self.idx]
					#tr = self.u.set_center_freq(freq)
					tr = self.u(freq)
					if tr is None:
						tr = True
					if not isinstance(tr, bool):
						print tr
						tr = True
					if tr:
						print "Set freq:", freq
					else:
						print "Failed to set freq:", freq
				time.sleep(self.delay)
			except Exception, e:
				print "Exception while hopping:", e
				traceback.print_exc(None, sys.stderr)
		self.stop_event.set()
	def stop(self):
		if not self.keep_running:
			return
		self.keep_running = False
		self.stop_event.wait()
		self.stop_event.clear()
		print "Stopped"
	def toggle(self, on):
		self.hop = on
	def set_delay(self, delay):
		self.delay = delay

#class hopper_hier(gr.hier_block2):
#	def __init__(self, *args, **kwds):
#		gr.hier_block2.__init__(self, "hopper",
#								gr.io_signature(0, 0, 0),
#								gr.io_signature(0, 0, 0))
#		self.thread = h.hopper_thread(*args, auto_start=False, hop=auto_start, **kwds)
#	def start(self):
#		print "Block start"
#		self.thread.start()
#	def stop(self):
#		self.thread.stop()
#	def __del__(self):
#		self.stop()
#	def set_freq(self, *args, **kwds):
#		return self.thread.set_freq(*args, **kwds)
#	def get(self, *args, **kwds):
#		return self.thread.get(*args, **kwds)

class hopper(gr.basic_block):
	def __init__(self, auto_start=True, *args, **kwds):
		self.auto_start = auto_start
		gr.basic_block.__init__(self, name="hopper", in_sig=None, out_sig=None)
		self.thread = hopper_thread(*args, auto_start=False, hop=auto_start, **kwds)
	def start(self):
		#if self.auto_start:
		#	print "Block start"
		self.thread.start()
		return True
	def stop(self):
		self.thread.stop()
		return True
	#def set_freq(self, *args, **kwds):
	#	return self.thread.set_freq(*args, **kwds)
	#def get(self, *args, **kwds):
	#	return self.thread.get(*args, **kwds)
	def toggle(self, on):
		self.thread.toggle(on)
	def set_delay(self, delay):
		self.thread.set_delay(delay)

def main():
	return 0

if __name__ == '__main__':
	main()
