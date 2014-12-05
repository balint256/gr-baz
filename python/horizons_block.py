#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  horizons_block.py
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

from gnuradio import gr

import horizons as h

class horizons_hier(gr.hier_block2):
	def __init__(self, *args, **kwds):
		gr.hier_block2.__init__(self, "horizons",
								gr.io_signature(0, 0, 0),
								gr.io_signature(0, 0, 0))
		self.thread = h.horizons_thread(*args, auto_start=False, **kwds)
	def start(self):
		print "horizons start"
		self.thread.start()
	def stop(self):
		self.thread.stop()
	def __del__(self):
		self.stop()
	def set_freq(self, *args, **kwds):
		return self.thread.set_freq(*args, **kwds)
	def get(self, *args, **kwds):
		return self.thread.get(*args, **kwds)

class horizons(gr.basic_block):
	def __init__(self, *args, **kwds):
		gr.basic_block.__init__(self, name="horizons", in_sig=None, out_sig=None)
		self.thread = h.horizons_thread(*args, auto_start=False, **kwds)
	def start(self):
		self.thread.start()
		return True
	def stop(self):
		self.thread.stop()
		return True
	def set_freq(self, *args, **kwds):
		return self.thread.set_freq(*args, **kwds)
	def get(self, *args, **kwds):
		return self.thread.get(*args, **kwds)

def main():
	return 0

if __name__ == '__main__':
	main()
