#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  correlator.py
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

import sys

import numpy
import matplotlib.pyplot as plt

from gnuradio import gr
import pmt

def find_peak(data, threshold, search_window, start=0):
	first = None
	cnt = 0
	for y in data[start:]:
		if y >= threshold:
			first = start + cnt
			break
		cnt += 1
	
	if first is None:
		return None, 0.0
	
	#first = max(0, first - search_window/2)
	
	first_data = data[first:min(first+search_window, len(data))]
	
	first += numpy.argmax(first_data)
	
	#return first, numpy.max(first_data)
	return first, 1.0

class correlator(gr.basic_block):
#class correlator(gr.sync_block):
	"""
	docstring for block correlator
	"""
	def __init__(self,
			samp_rate,
			symbol_rate,
			window_length,
			threshold=0.5,
			width=1024,
			sync_path='sync.dat',
			sync_length=511,
			sync_offset=50,
			sync_dtype='c8',
			sync_window_length=500):
		gr.basic_block.__init__(self,
		#gr.sync_block.__init__(self,
			name="correlator",
			in_sig =[numpy.complex64],
			out_sig=[numpy.float32])		# Cannot define a vector
		
		self.set_output_multiple(width)	# Fake vector
		
		relative_rate = 1.*width / (samp_rate / symbol_rate * window_length)
		print "Relative rate:", relative_rate
		self.set_relative_rate(relative_rate)
		
		print "Sample rate: %f" % (samp_rate)
		print "Symbol rate: %f" % (symbol_rate)
		print "Window length: %d" % (window_length)
		print "Threshold: %f" % (threshold)
		print "Width: %d" % (width)
		print "Sync window length: %d" % (sync_window_length)
		
		self.threshold = threshold
		self.sync_window_length = sync_window_length
		self.width = width
		self.window_length = window_length
		
		print "Reading sync:", sync_path
		sync_dtype = numpy.dtype(sync_dtype)
		self.sync_data = numpy.fromfile(sync_path, sync_dtype, (sync_offset + sync_length))[sync_offset:]
		print "Read %d items" % (len(self.sync_data))
		
		self.synced = False
		self.data = numpy.array([])
		self.idx = 0
		self.next_window_idx = None
		self.work_count = 0
		self.prev_peak_idx = None
		self.scanlines = numpy.array([])
	
	#def forecast(self, noutput_items, ninput_items_required):
	#	for i in range(len(ninput_items_required)):
	#		ninput_items_required[i] = self.window_length	#noutput_items
	
	def work(self, input_items, output_items):
		raise Exception()
		if len(input_items[0]):
			#print len(input_items[0]),
			self.consume(0, len(input_items[0]))
		else:
			sys.stdout.write('.')
			sys.stdout.flush()
		return len(output_items[0])
	
	def general_work(self, input_items, output_items):
		##raise Exception()
		#assert(len(input_items) == 1)
		#assert(len(output_items) == 1)
		#if len(input_items[0]):
		#	print len(input_items[0]),
		#	self.consume(0, len(input_items[0]))
		#	self.consume_each(len(input_items[0]))
		#	pass
		#else:
		#	sys.stdout.write('.')
		#	sys.stdout.flush()
		#return len(output_items[0])
		#return 0
		
		#print len(output_items[0])
		
		self.work_count += 1
		
		residual_data = len(self.data)
		if residual_data == 0:
			self.data = input_items[0]
		else:
			self.data = numpy.append(self.data, [input_items[0]])
		
		padding = len(self.sync_data)	# MAGIC/FIXME
		
		if not self.synced:
			if len(self.data) >= (len(self.sync_data) + 0):	 # FIXME: Probably needs to be longer
				corr = numpy.correlate(self.data, self.sync_data, mode='same')
				corr_mag = numpy.abs(corr)
				peak_idx, peak_mag = find_peak(corr_mag, self.threshold, self.sync_window_length)
				#peak_idx, peak_mag = None, 0.0
				if peak_idx is None:
					#print "Failed to find first peak in %d samples" % (len(self.data))
					self.data = self.data[-len(self.sync_data):]
					assert(len(self.data) == len(self.sync_data))
					self.idx -= len(self.data)	# Will be added to below
				else:
					self.prev_peak_idx = self.idx + peak_idx
					print "Found first peak: %f (relative sample: %d, global index: %d)" % (peak_mag, peak_idx, self.prev_peak_idx)
					
					self.synced = True
					
					self.next_window_idx = self.prev_peak_idx + self.window_length - self.width/2
					#print "Next window index: %d" % (self.next_window_idx)
					
					consumed = peak_idx - residual_data
					self.consume(0, consumed)
					self.idx += peak_idx
					self.data = numpy.array([])
					
					return 0
		else:
			if (self.idx + len(input_items[0])) > (self.next_window_idx + self.width + padding):	# Adding sync length for extra
				start_idx = (self.idx + len(input_items[0])) - self.next_window_idx
				assert(start_idx >= 0)
				data_start = len(self.data) - start_idx - padding/2
				assert(data_start >= 0)
				data_length = self.width+padding
				assert(data_start < len(self.data))
				assert((data_start+data_length) <= len(self.data))
				
				corr = numpy.correlate(self.data[data_start:data_start+data_length], self.sync_data, mode='same')
				corr_mag = numpy.abs(corr)
				peak_idx, peak_mag = find_peak(corr_mag, self.threshold, self.sync_window_length)
				
				#corr_mag = numpy.zeros(data_length)
				#peak_idx, peak_mag = data_length/2, 1.0
				
				if peak_idx is None:
					print "#%04d Failed to find expected peak (next window index: %d)" % (self.work_count, self.next_window_idx)
					self.synced = False
					peak_idx = len(corr_mag)/2
					peak_mag = numpy.max(corr_mag)
				else:
					#print "#%04d Found peak: %f (relative sample: %d, buffer size: %d)" % (self.work_count, peak_mag, peak_idx, len(self.data))
					pass
				
				if self.synced:
					global_peak_idx = (self.idx + len(input_items[0])) - len(self.data) + data_start + peak_idx
					diff = self.window_length - (global_peak_idx - self.prev_peak_idx)
					#print "#%04d Difference1: %d" % (self.work_count, diff)
					self.prev_peak_idx = global_peak_idx
					
					if True:
						diff = data_length/2 - peak_idx
						#print "#%04d Difference2: %d" % (self.work_count, diff)
						self.next_window_idx += self.window_length - diff
						
						#offset = data_start + padding/2 + self.width/2
						offset = data_start + peak_idx + self.window_length - self.width/2
						if offset >= len(self.data):
							self.data = numpy.array([])
						else:
							self.data = self.data[offset:]
						#self.idx += len(input_items[0]) - offset
						self.idx += len(input_items[0]) - len(self.data)
						self.consume(0, len(input_items[0]))
						
						output_start = peak_idx-self.width/2
						#output_items[0] = corr_mag[output_start:output_start+self.width]
						#print "Output items: %d" % (len(output_items[0]))
						assert(len(output_items[0]) >= self.width)
						to_copy = corr_mag[output_start:output_start+self.width]
						numpy.copyto(output_items[0], numpy.append(to_copy, [numpy.zeros(len(output_items[0]) - len(to_copy))]))
						#for i in range(self.width):
						#	output_items[0][i] = to_copy[i]
						#output_items[0][:] = corr_mag[output_start:output_start+self.width]
						#self.scanlines = numpy.append(self.scanlines, [output_items[0]])	#[corr_mag[padding/2:padding/2+self.width]])
						#assert(len(output_items[0]) == self.width)
						
						return self.width
			#else:
			#	print "#%04d Waiting for next window at %d (now at %d). Data buffer length: %d" % (self.work_count, self.next_window_idx, self.idx, len(self.data))
		
		consumed = len(input_items[0])
		self.idx += consumed
		self.consume(0, consumed)
		
		return 0
	
	def stop(self):
		return True
		
		print len(self.scanlines)
		print self.width
		line_count = len(self.scanlines) / self.width
		self.scanlines = self.scanlines.reshape((line_count, self.width))
		print self.scanlines.shape
	
		scanline_plot = plt.imshow(self.scanlines)
		plt.show()
		
		return True

def main():
	return 0

if __name__ == '__main__':
	main()
