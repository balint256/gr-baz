#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  rx_time.py
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

#import math
import numpy

from gnuradio import gr
import pmt

class rx_time(gr.sync_block):
	"""
	docstring for rx_time
	"""
	def __init__(self, item_size):
		# FIXME: item_size
		dtype = numpy.int32
		gr.sync_block.__init__(self,
			name	= "rx_time",
			in_sig	= [dtype],
			out_sig	= None)
		
		self.time = None
		
		#print "<%s[%s]> item size: %d" % (self.name(), self.unique_id(), item_size)
	
	def work(self, input_items, output_items):
		nread = self.nitems_read(0)
		items = len(input_items[0])
		#print nread, items
		tags = self.get_tags_in_range(0, nread, nread + items)
		
		for t in tags:
			tag = gr.tag_to_python(t)
			#print t, tag
			#print dir(tag)
			#'key', 'offset', 'srcid', 'value'
			#print tag.offset, tag.srcid, tag.key, tag.value
			if tag.key == 'rx_time':
				whole_sec = tag.value[0]
				frac_sec = tag.value[1]
				self.time = whole_sec + frac_sec
				#print "[%s] %s: %f" % (tag.offset, tag.value, self.time)
				return -1
		
		self.consume_each(items)
		
		return 0

def main():
	return 0

if __name__ == '__main__':
	main()
