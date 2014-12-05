#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  multi_channel_decoder.py
#  
#  Copyright 2013 Balint Seeber <balint256@gmail.com>
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
from gnuradio import gr, gru
from baz import message_relay

class multi_channel_decoder(gr.hier_block2):
	def __init__(self, msgq, baseband_freq, frequencies, decoder, decoder_args=None, params={}, per_freq_params={}, **kwargs):
		gr.hier_block2.__init__(self, "multi_channel_decoder",
			gr.io_signature(1, 1, gr.sizeof_gr_complex),
			gr.io_signature(0, 0, 0))
		
		self.msgq = msgq
		self.decoder = decoder
		self.decoder_args = decoder_args or ""
		self.params = params
		self.kwargs = kwargs
		self.per_freq_params = per_freq_params
		
		self.decoders = []
		self.decoders_unused = []
		
		self.set_baseband_freq(baseband_freq)
		
		self.set_frequencies(frequencies, True)
	
	def set_frequencies(self, freq_list, skip_lock=False):	# FIXME: Filter duplicate frequencies / connect Null Sink if freq list is empty / Lock
		current_freqs = []
		map_freqs = {}
		for decoder in self.decoders:
			current_freqs += [decoder.get_freq()]
			map_freqs[decoder.get_freq()] = decoder
		create = [f for f in freq_list if f not in current_freqs]
		remove = [f for f in current_freqs if f not in freq_list]
		if not skip_lock: self.lock()
		try:
			decoder_factory = self.decoder
			if isinstance(self.decoder, str):
				decoder_factory = eval(self.decoder)
			#factory_eval_str = "decoder_factory(baseband_freq=%s,freq=%f,%s)" % (self.baseband_freq, f, self.decoder_args)
			for f in create:
				#d = eval(factory_eval_str)
				combined_args = self.kwargs
				combined_args['baseband_freq'] = self.baseband_freq
				combined_args['freq'] = f
				if f in self.per_freq_params:
					for k in self.per_freq_params[f].keys():
						combined_args[k] = self.per_freq_params[f][k]
				print "==> Creating decoder:", decoder_factory, "with", combined_args
				#d = decoder_factory(baseband_freq=self.baseband_freq, freq=f, **combined_args)
				d = decoder_factory(**combined_args)
				d._msgq_relay = message_relay.message_relay(self.msgq, d.msg_out.msgq())
				self.connect(self, d)
				self.decoders += [d]
		except Exception, e:
			print "Failed to create decoder:", e#, factory_eval_str
		try:
			for f in remove:
				decoder = map_freqs[f]
				print "Disconnecting decoder for %f" % (decoder.get_freq())
				self.disconnect(self, decoder)
				self.decoders.remove(decoder)
				#self.decoders_unused += [decoder]	# FIXME: Re-use mode
		except Exception, e:
			print "Failed to remove decoder:", e
		if not skip_lock: self.unlock()
	
	def set_baseband_freq(self, baseband_freq):
		self.baseband_freq = baseband_freq
		for decoder in self.decoders:
			decoder.set_baseband_freq(baseband_freq)
	
	def update_parameters(self, params):
		for k in params:
			if k not in self.params or self.params[k] != params[k]:	# Only update those that don't exist yet or have changed
				print "Updating parameter:", k, params[k]
				for decoder in self.decoders:
					try:
						fn = getattr(decoder, k)
						fn(params[k])
					except Exception, e:
						print "Exception updating parameter in:", decoder, k, params[k]
						traceback.print_exc()
				self.params[k] = params[k]

def main():
	return 0

if __name__ == '__main__':
	main()
