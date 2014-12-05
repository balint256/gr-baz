#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  realtime_fft.py
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

import sys, struct, socket, traceback, os, fcntl, time, thread, threading

from optparse import OptionParser

import numpy

from fft_tools import *
from realtime_graph import *
from utils import *

class data_receiver(threading.Thread):
	def __init__(self, s, options, graph, update_event, **kwds):
		threading.Thread.__init__(self, **kwds)
		self.setDaemon(True)
		self.s = s
		self.options = options
		self.graph = graph
		self.keep_running = True
		self.buffer = ""
		self.graph = graph
		self.update_event = update_event
		self.dtype = numpy.dtype("c8")
		self.fft_buffer_length = options.fft_length * self.dtype.itemsize	# Hardcoded to 2*sizeof(float)
		#self.start()
	def run(self):
		if self.s:
			while self.keep_running:
				data, addr = self.s.recvfrom(self.options.buffer_size)
				if self.keep_running == False:
					break
				if len(data) == 0:
					break
				try:
					self.buffer += data
					while len(self.buffer) >= self.fft_buffer_length:
						samples = numpy.frombuffer(self.buffer[:self.fft_buffer_length], self.dtype)
						#print len(samples)
						num_ffts, fft_avg, fft_min, fft_max = calc_fft(samples, self.options.fft_length, log_scale=True)	#, window=None
						#freqs = numpy.linspace(freq_min, freq_max, len(fft_avg))
						#self.graph.update(data=fft_avg, redraw=False)	# , x=freqs, points=spurs_detected
						self.graph.set_data(data=fft_avg, autoscale=False)	# , auto_x_range=False
						self.update_event.set()
						self.buffer = self.buffer[self.fft_buffer_length:]
				except:
					pass

def main():
	parser = OptionParser(usage="%prog: [options]")
	
	parser.add_option("-p", "--port", type="int", default=11111, help="port [default=%default]")
	parser.add_option("-b", "--buffer-size", type="int", default=1024*2, help="receive buffer size [default=%default]")
	parser.add_option("-l", "--fft-length", type="int", default=256*1024, help="fft length [default=%default]")
	parser.add_option("-f", "--freq", type="float", default=0.0, help="centre frequency [default=%default]")
	parser.add_option("-s", "--samp-rate", type="float", default=250e3, help="sample rate [default=%default]")
	parser.add_option("-I", "--listen", type="string", default="0.0.0.0", help="listen interface IP [default=%default]")
	parser.add_option("-L", "--lower-limit", type="float", default=-130, help="Lower amplitude limit [default=%default]")
	parser.add_option("-U", "--upper-limit", type="float", default=0, help="Upper amplitude limit [default=%default]")
	#parser.add_option("-P", "--pipe", action="store_true", default=False, help="use a pipe instead of a socket [default=%default]")
	#parser.add_option("-L", "--limit", type="int", default=(2**16-1), help="limit [default=%default]")
	#parser.add_option("-i", "--interval", type="int", default=1024, help="update interval [default=%default]")
	#parser.add_option("-l", "--listen", action="store_true", default=False, help="listen on a socket instead of connecting to a server [default=%default]")
	
	(options, args) = parser.parse_args()
	
	s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	s.bind((options.listen, options.port))
	
	channel_count = 1
	channels_to_show = [0]
	default_data = numpy.ones(options.fft_length)
	
	freq_min = options.freq - options.samp_rate/2
	freq_max = options.freq + options.samp_rate/2
	
	####################################
	
	font = {
		#'family' : 'normal',
		#'weight' : 'bold',
		'size'   : 10
	}
	
	matplotlib.rc('font', **font)
	
	padding = 0.05
	spacing = 0.1
	figure_width = 8
	figure_height = 10
	
	if channel_count > 2:
		channel_pos = 220
		figure_width = figure_width * 2
	elif channel_count == 2:
		channel_pos = 210
	else:
		channel_pos = 110
	
	figsize = (figure_width, figure_height)
	padding = {'wspace':spacing,'hspace':spacing,'top':1.-padding,'left':padding,'bottom':padding,'right':1.-padding}
	fft_graph = realtime_graph(title="FFT", show=True, manual=True, redraw=False, figsize=figsize, padding=padding)
	fft_channel_graphs = {}
	
	pos_count = 0
	y_limits = (options.lower_limit, options.upper_limit)
	for channel_idx in channels_to_show:
		#if channel_count > 2:
		#    pos_offset = ((pos_count % 2) * 2) + (pos_count / 2) + 1   # Re-order column-major
		#else:
		pos_offset = pos_count + 1
		subplot_pos = (channel_pos + pos_offset)
		
		x = numpy.linspace(freq_min, freq_max, options.fft_length)
		
		fft_channel_graphs[channel_idx] = sub_graph = realtime_graph(
			parent=fft_graph,
			show=True,
			redraw=False,
			sub_title="Channel %i" % (channel_idx),
			pos=subplot_pos,
			y_limits=y_limits,
			#x_range=options.fft_length,
			#data=[fft_avg, fft_min, fft_max],
			data = default_data,
			x=x)
		
		#for freq in freq_line:
		#	sub_graph.add_vert_line(freq, 'gray')
		#for freq in freq_center_line:
		#	sub_graph.add_vert_line(freq)
	
	###########
	
	update_event = threading.Event()
	
	receiver = data_receiver(s, options, fft_channel_graphs[0], update_event)
	
	receiver.start()
	
	#fft_graph.go_modal()
	
	try:
		while True:
			fft_graph.run_event_loop(0.001)
			if update_event.isSet():
				update_event.clear()
				fft_graph.redraw()
	except KeyboardInterrupt:
		pass
	except Exception, e:
		print "Unexpected exception:", e
	
	return 0

if __name__ == '__main__':
	main()
