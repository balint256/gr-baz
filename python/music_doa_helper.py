#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  music_doa.py
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

from gnuradio import gr, gru
import numpy
import baz

def unit_vect(theta):
    return numpy.array([numpy.cos(theta), numpy.sin(theta)])

def calculate_antenna_array_response(antenna_array, angular_resolution, l):
	response = []
	#for angle in xrange(0.0, 360.0*((angular_resolution-1)/angular_resolution), (360.0/angular_resolution)):
	for step in xrange(0, angular_resolution):
		angle = (step * 360.0 / angular_resolution) * (numpy.pi / 180.0)
		
		response_step = []
		for antenna in antenna_array:
			phase_offset = numpy.inner(antenna, unit_vect(angle)) / l	# NEGATIVISM # -angle - numpy.pi/2.0
			antenna_response = numpy.exp(-1j * 2.0 * numpy.pi * phase_offset)			# NEGATIVISM
			response_step += [antenna_response]
		
		response += [response_step]
	
	return response

class music_doa_helper(gr.hier_block2):
	def __init__(self, m, n, nsamples, angular_resolution, frequency, array_spacing, antenna_array, output_spectrum=False):
		
		self.m = m
		self.n = n
		self.nsamples = nsamples
		self.angular_resolution = angular_resolution
		self.l = 299792458.0 / frequency
		self.antenna_array = [[array_spacing * x, array_spacing * y] for [x,y] in antenna_array]
		
		if (nsamples % m) != 0:
			raise Exception("nsamples must be multiple of m")
		
		if output_spectrum:
			output_sig = gr.io_signature3(3, 3, (gr.sizeof_float * n), (gr.sizeof_float * n), (gr.sizeof_float * angular_resolution))
		else:
			output_sig = gr.io_signature2(2, 2, (gr.sizeof_float * n), (gr.sizeof_float * n))
		#	output_sig = gr.io_signature2(2, 2, (gr.sizeof_float * n), (gr.sizeof_float * angular_resolution))
		#else:
		#	output_sig = gr.io_signature(1, 1, (gr.sizeof_float * n))
		
		gr.hier_block2.__init__(self, "music_doa_helper",
								gr.io_signature(1, 1, (gr.sizeof_gr_complex * nsamples)),
								output_sig)
								#gr.io_signature3(1, 3,	(gr.sizeof_float * n),
								#						(gr.sizeof_float * angular_resolution),
								#						(gr.sizeof_float * angular_resolution)))
		
		print "MUSIC DOA Helper: M: %d, N: %d, # samples: %d, steps of %f degress, lambda: %f, array: %s" % (
			self.m,
			self.n,
			self.nsamples,
			(360.0/self.angular_resolution),
			self.l,
			str(self.antenna_array)
		)
		
		#print "--> Calculating array response..."
		self.array_response = calculate_antenna_array_response(self.antenna_array, self.angular_resolution, self.l)
		#print "--> Done."
		#print self.array_response
		self.impl = baz.music_doa(self.m, self.n, self.nsamples, self.array_response, self.angular_resolution)
		
		self.connect(self, self.impl)
		
		self.connect((self.impl, 0), (self, 0))
		self.connect((self.impl, 1), (self, 1))
		if output_spectrum:
			self.connect((self.impl, 2), (self, 2))
		#if
		#self.connect((self.impl, 2), (self, 2))
	
	def set_frequency(self, frequency):
		self.l = 299792458.0 / frequency
		self.array_response = calculate_antenna_array_response(self.antenna_array, self.angular_resolution, self.l)
		self.impl.set_array_response(self.array_response)
