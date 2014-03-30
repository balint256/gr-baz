#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  message_relay.py
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

import threading

from gnuradio import gr

class queue_watcher(threading.Thread):
	def __init__(self, tx_msgq, rx_msgq, **kwds):
		threading.Thread.__init__(self, **kwds)
		self.setDaemon(True)
		self.tx_msgq = tx_msgq
		self.rx_msgq = rx_msgq
		self.keep_running = True
		self.start()
	def run(self):
		if self.rx_msgq:
			while self.keep_running:
				msg = self.rx_msgq.delete_head()
				if self.keep_running == False:
					break
				try:
					if self.tx_msgq:
						self.tx_msgq.insert_tail(msg)
						#if self.tx_msgq.full_p():
						#	print "==> Dropping message!"
						#else:
						#	self.tx_msgq.handle(msg)
						#print "==> Message relay forwarded message"
				except:
					pass

class message_relay():
	def __init__(self, tx_msgq, rx_msgq):
		if tx_msgq is None:
			raise Exception, "message_relay requires a TX message queue"
		if rx_msgq is None:
			raise Exception, "message_relay requires a RX message queue"
		self._tx_msgq = tx_msgq
		if isinstance(rx_msgq, str):
			rx_msgq = eval(str)
		self._rx_msgq = rx_msgq
		self._watcher = queue_watcher(tx_msgq=tx_msgq, rx_msgq=rx_msgq)
	def __del__(self):
		self.stop()
	def stop(self):
		if self._watcher is None:
			return
		self._watcher.keep_running = False
		msg = gr.message()  # Empty message to signal end
		self._rx_msgq.insert_tail(msg)
	def tx_msgq(self):
		return self._tx_msgq
	def rx_msgq(self):
		return self._rx_msgq

def main():
	return 0

if __name__ == '__main__':
	main()
