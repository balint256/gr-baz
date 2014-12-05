#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  papr.py
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

import sys, math
import numpy
import matplotlib.pyplot as pyplot
from optparse import OptionParser

def moving_average(values, window):
	weights = numpy.repeat(1.0, window) / window
	sma = numpy.convolve(values, weights, 'same')	# 'valid'
	#print len(values), len(sma)
	return sma

def main():
	parser = OptionParser(usage="%prog: [options] <input file>")
	
	parser.add_option("-t", "--type", type="string", default="c8", help="data type [default=%default]")
	parser.add_option("-T", "--trim", type="int", default=None, help="max # of samples to use [default=%default]")
	parser.add_option("-d", "--decim", type="int", default=None, help="decimation [default=%default]")
	parser.add_option("-l", "--length", type="int", default="2048", help="target window length [default=%default]")
	parser.add_option("-a", "--average", type="int", default="128", help="moving average window length [default=%default]")
	parser.add_option("-m", "--max-length", type="int", default="1024", help="max window length [default=%default]")
	parser.add_option("-D", "--max-decim", type="int", default=None, help="max decimation [default=%default]")
	parser.add_option("-L", "--log", action="store_true", default=False, help="log scale [default=%default]")
	parser.add_option("-M", "--show-mag", action="store_true", default=False, help="show magnitude plot [default=%default]")
	#parser.add_option("-T", "--mag-trim", type="int", default=None, help="max # of samples to show in mag plot [default=%default]")
	
	(options, args) = parser.parse_args()
	
	if len(args) < 1:
		print "Supply input file"
		return
	
	input_file = args[0]
	
	dtype = numpy.dtype(options.type)
	
	print "Opening", input_file, "as", dtype
	data = numpy.fromfile(input_file, dtype)
	print "File samples:", len(data)
	
	if options.trim is not None:
		print "Trimming to", options.trim
		data = data[:options.trim]
	
	print "Min,mean,max:", data.min(), data.mean(), data.max()
	
	if options.decim is not None:
		decim = options.decim	# FIXME: Validate
	else:
		decim = len(data) / options.length
	print "Decim:", decim
	new_length = decim * options.length
	print "New length:", new_length, ", skipping:", (len(data) - new_length)
	data = data[:new_length]
	
	data_mag = numpy.abs(data)
	data_mag_min = data_mag.min()
	if data_mag_min == 0.0:
		print "Mag min: %f" % (data_mag_min)
	else:
		print "Mag min: %f (%f dB)" % (data_mag_min, 10.0*math.log10(data_mag_min))
	data_mag_mean = data_mag.mean()
	print "Mag mean: %f (%f dB)" % (data_mag_mean, 10.0*math.log10(data_mag_mean))
	data_mag_max = data_mag.max()
	print "Mag max: %f (%f dB)" % (data_mag_max, 10.0*math.log10(data_mag_max))
	
	data_mag_squared = data_mag ** 2.0
	mean_rms = math.sqrt(data_mag_squared.mean())
	print "Mean RMS:", mean_rms, "(%f dB)" % (10.0*math.log10(mean_rms))
	print "Moving average window length:", options.average
	data_mag_squared_ma = moving_average(data_mag_squared, options.average)
	#print len(data_mag_ma)
	#len_diff = new_length - len(data_mag_ma)
	
	#if options.decim is not None:
	#	decim = len(data_mag_ma) / options.length
	#else:
	#	decim = len(data) / options.length
	#print "Decim:", decim
	#new_length = decim * options.length
	#print "New length:", new_length
	#data_mag_ma = data_mag_ma[:new_length]
	
	#print "Moving average decim:", decim
	#new_length = decim * options.length
	#data_mag_ma = data_mag_ma[:new_length]
	#print "New length:", len(data_mag_ma)
	
	if decim > 1:
		data_mag_ma_mat = data_mag_squared_ma.reshape(-1, decim)
		data_mag_ma_mean = data_mag_ma_mat.mean(axis=1)
	else:
		data_mag_ma_mean = data_mag_squared_ma
	
	print "Mean moving-average data length:", len(data_mag_ma_mean)
	print "Min,mean,max: %f, %f, %f" % (data_mag_ma_mean.min(), data_mag_ma_mean.mean(), data_mag_ma_mean.max())
	
	if options.max_decim is None:
		assert((new_length % options.max_length) == 0)
		decim_max = new_length / options.max_length
	else:
		decim_max = options.max_decim
	print "Max decim:", decim_max
	#new_length_max = decim_max * options.max_length
	
	#data_mag_decim = data_mag[new_length/2:]
	#len_diff = len(data_mag_decim) - new_length
	#data_mag_decim = data_mag_decim[:-len_diff+1]
	data_mag_decim = data_mag
	
	if decim_max > 1:
		data_mag_decim_mat = data_mag_decim.reshape(-1, decim_max)
		data_mag_decim_max = data_mag_decim_mat.max(axis=1)
	else:
		data_mag_decim_max = data_mag_decim
	
	repeat = options.length / options.max_length
	print "Max repeat:", repeat
	if repeat > 1:
		data_mag_decim_max = numpy.repeat(data_mag_decim_max, repeat)
	
	print "Min,mean,max: %f, %f, %f" % (data_mag_decim_max.min(), data_mag_decim_max.mean(), data_mag_decim_max.max())
	
	data_mag_decim_max_squared = data_mag_decim_max ** 2.0
	
	ratio = data_mag_decim_max_squared / data_mag_ma_mean
	ratio_filtered = ratio[~numpy.isnan(ratio)]
	print "NaNs:", (len(ratio) - len(ratio_filtered))
	ratio_filtered2 = ratio_filtered[~numpy.isinf(ratio_filtered)]
	print "Infs:", (len(ratio_filtered) - len(ratio_filtered2))
	print "Min,mean,max: %f, %f, %f" % (ratio_filtered2.min(), ratio_filtered2.mean(), ratio_filtered2.max())
	#print ratio
	#print ratio_filtered
	
	orig_ratio_len = len(ratio)
	#trim = options.average - 1
	#ratio = ratio[trim:-trim]
	trim = 0
	x = numpy.linspace(trim, trim + len(ratio), len(ratio), False)
	#print len(x), len(ratio)
	
	mean_ratio = ratio_filtered2.mean()
	print "Mean ratio:", mean_ratio, "(%f dB)" % (10.0*math.log10(mean_ratio))
	
	ratio_db = 10.0 * numpy.log10(ratio)
	ratio_filtered_db = 10.0 * numpy.log10(ratio_filtered2)
	print "Min,mean,max ratio (dB): %f, %f, %f" % (ratio_filtered_db.min(), ratio_filtered_db.mean(), ratio_filtered_db.max())
	
	if options.show_mag:
		subplot = pyplot.subplot(111)
		subplot.grid(True)
		
		print "Showing magnitude plot..."
		#subplot.set_ylim(ymin=0.0)
		#subplot.plot(data)
		
		subplot.plot(data_mag)
		
		data_mag_rms_ma = data_mag_squared_ma ** 0.5
		subplot.plot(data_mag_rms_ma)
		
		data_mag_rms_ma_mean = numpy.repeat(data_mag_ma_mean, decim) ** 0.5
		subplot.plot(data_mag_rms_ma_mean)
		
		data_mag_decim_max_repeat = numpy.repeat(data_mag_decim_max, decim)
		subplot.plot(data_mag_decim_max_repeat)
		
		pyplot.show()
	
	subplot = pyplot.subplot(111)
	if options.log:
		subplot.set_yscale('log')
	subplot.grid(True)
	
	#subplot.set_ylim(ymin=(10.0**-18.), ymax=(10.0**-8.))
	#plot, = subplot.plot(data_mag_mean)
	#plot, = subplot.plot(data_mag_decim_max)
	
	print "Showing PAPR plot..."
	subplot.set_ylim(ymin=0.0, ymax=ratio_filtered_db.max())
	subplot.set_xlim(xmax=orig_ratio_len)
	plot, = subplot.plot(x, ratio_db)
	
	pyplot.show()
	
	return 0

if __name__ == '__main__':
	main()
