#!/usr/bin/env python
# 
# Copyright 2016,2017 Balint Seeber
# 
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
# 
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
# 

import time, math

from gnuradio import gr
import pmt

class simple_synchroniser(gr.basic_block):
	"""
	docstring for block simple_synchroniser
	"""
	def __init__(self, target_period, alpha=1.0, jump_ratio=None, lock_sd=None, ratio=1.0, window_length=100, limit=None, verbose=False):
		gr.basic_block.__init__(self,
			name="simple_synchroniser",
			in_sig=None,
			out_sig=None)
		
		self.target_period = target_period
		self.alpha = alpha
		self.jump_ratio = jump_ratio
		self.lock_sd = lock_sd
		self.ratio = ratio
		self.init_ratio = ratio
		self.window_length = window_length
		self.limit = limit
		self.verbose = verbose

		print "Target period:", target_period

		self.locked = False
		self.window = []

		self.message_port_register_out(pmt.intern('out'))
		self.message_port_register_in(pmt.intern('in'))
		self.set_msg_handler(pmt.intern('in'), self.handle)

	def general_work(self, input_items, output_items):
		print "general_work"
		return -1

	def handle(self, msg):
		meta = pmt.to_python(msg)
		if not isinstance(meta, dict):
			print "Unexpected type:", meta
			return

		period = meta['period']

		new_ratio = period / self.target_period

		if self.limit is not None:
			if (new_ratio > (self.init_ratio * (1.0 + self.limit))) or (new_ratio < (self.init_ratio * (1.0 - self.limit))):
				if self.verbose:
					print "Exceeds limit: {} ({})".format(period, new_ratio)
				return

		ratio_diff = abs((new_ratio / self.ratio) - 1.0)

		avg_ratio = (self.alpha * new_ratio) + ((1.0 - self.alpha) * self.ratio)

		# if self.verbose:
		# 	print "Period: {}, ratio: {}, ratio diff: {}".format(period, new_ratio, ratio_diff)

		if self.jump_ratio is not None and ratio_diff >= self.jump_ratio:
			print "Jumping to ratio: {}", new_ratio
			self.ratio = new_ratio
			if self.locked:
				print "Unlocked"
			self.locked = False
			self.window = []
		elif not self.locked:
			self.ratio = avg_ratio

		if len(self.window) >= self.window_length:
			self.window = self.window[(len(self.window) - self.window_length + 1):]

		self.window.append(avg_ratio)

		_sum = sum(self.window)
		_count = float(len(self.window))
		_ave = _sum / _count
		_sum_squared = sum([x*x for x in self.window])

		_var = ( _sum_squared + (_ave  * ((_ave * _count) - 2.0 * _sum) ) ) / _count
		if _var < 0: # Due to floating point error
			# print "Var negative: {}".format(_var)
			_var = 0
		_sd = math.sqrt(_var)

		if self.lock_sd is not None:
			if len(self.window) == self.window_length:
				if not self.locked:
					if _sd <= self.lock_sd:
						self.locked = True
						print "Locked"

		new_ratio_ppb = self.ratio * 1e9
		if self.verbose:
			print "New ratio: {} (#{}, SD: {}), reported period: {} (ratio: {}), ratio diff: {}, locked: {}".format(self.ratio, len(self.window), _sd, period, new_ratio, ratio_diff, self.locked)
		new_ratio_int = int(new_ratio_ppb)
		new_ratio_frac = new_ratio_ppb - new_ratio_int
		#self.message_port_pub(pmt.intern('out'), pmt.from_double(float(self.ratio)))
		self.message_port_pub(pmt.intern('out'), pmt.cons(pmt.from_long(new_ratio_int), pmt.from_double(float(new_ratio_frac))))
