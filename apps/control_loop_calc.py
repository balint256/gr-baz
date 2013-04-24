#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  control_loop_calc.py
#  
#  Copyright 2013 Balint Seeber <balint@ettus.com>
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

import math
from optparse import OptionParser
from gnuradio.eng_option import eng_option

def main():
	usage="%prog: [options]"
	parser = OptionParser(option_class=eng_option, usage=usage)
	parser.add_option("-a", "--alpha", type="float", default=None, help="Alpha [default=%default]")
	parser.add_option("-b", "--beta", type="float", default=None, help="Beta [default=%default]")
	parser.add_option("-l", "--loop-bandwidth", type="float", default=None, help="Loop bandwidth [default=%default]")
	parser.add_option("-d", "--damping", type="float", default=None, help="Damping [default=%default]")
	(options, args) = parser.parse_args()
	if options.alpha is not None and options.beta is not None:
		bw = math.sqrt(options.beta / (4 - 2*options.alpha - options.beta))
		d = (options.alpha * (-1 - bw*bw)) / (2 * bw * (options.alpha - 2))
		print "Bandwidth:\t%f\nDamping:\t%f" % (bw, d)
	if options.loop_bandwidth is not None and options.damping is not None:
		denom = (1 + 2 * options.damping * options.loop_bandwidth + options.loop_bandwidth*options.loop_bandwidth)
		a = (4 * options.damping * options.loop_bandwidth) / denom
		b = (4 * options.loop_bandwidth*options.loop_bandwidth) / denom
		print "Alpha:\t%f\nBeta:\t%f" % (a, b)
	if options.alpha is not None and options.damping is not None:
		b = 2 * options.alpha * options.damping - 4 * options.damping
		p1 = -b
		p2 = math.sqrt(b*b - 4 * options.alpha*options.alpha)
		denom = 2 * options.alpha
		x1 = (p1 + p2) / denom
		x2 = (p1 - p2) / denom
		print "Bandwidth 1:\t%f\nBandwidth 2:\t%f" % (x1, x2)
	if options.beta is not None and options.damping is not None:
		b = -2 * options.beta * options.damping
		p1 = -b
		p2 = math.sqrt(b*b - 4 * (4 - options.beta) * (-options.beta))
		denom = 2 * (4 - options.beta)
		x1 = (p1 + p2) / denom
		x2 = (p1 - p2) / denom
		print "Bandwidth 1:\t%f\nBandwidth 2:\t%f" % (x1, x2)
	return 0

if __name__ == '__main__':
	main()
