#!/usr/bin/env python

"""
Provides the Legacy USRP interface via UHD
WARNING: This interface is only very basic!

http://wiki.spench.net/wiki/gr-baz
By Balint Seeber (http://spench.net/contact)
"""

from __future__ import with_statement

import time, os, sys, threading, thread
from string import split, join

from gnuradio import gr, gru, uhd

_prefs = gr.prefs()
_default_address = _prefs.get_string('legacy_usrp', 'address', '')

def pick_rx_subdevice(u):
	return (1, 0)

def determine_rx_mux_value(u, subdev_spec, subdev_spec_b=None):
	return 0

def selected_subdev(u, subdev_spec):	# Returns subdevice object
	return u.selected_subdev(subdev_spec)

class source_c(gr.hier_block2):
	"""
	Legacy USRP via UHD
	Assumes 64MHz clock in USRP 1
	'address' as None implies check config for default
	"""
	def __init__(self, address=None, which=0, decim_rate=0, adc_freq=64e6, defer_creation=True):	# FIXME: 'which'
		"""
		UHD USRP Source
		"""
		gr.hier_block2.__init__(self, "uhd_usrp_wrapper",
			gr.io_signature(0, 0, 0),
			gr.io_signature(1, 1, gr.sizeof_gr_complex))	# FIXME: Sample format
		
		self._decim_rate = decim_rate
		self._address = address
		self._which = which
		
		self._adc_freq = int(adc_freq)
		self._gain_range = (0, 1)
		self._tune_result = None
		self._name = "(Legacy USRP)"
		self._gain_step = 0
		self._created = False
		
		self._last_address = address
		self._last_which = None
		self._last_subdev_spec = None
		
		self._reset_last_params()
		
		self._uhd_usrp_source = None
		
		if defer_creation == False:	# Deferred until 'selected_subdev' is called
			self.create()
	
	def _reset_last_params(self):
		self._last_freq = None
		self._last_gain = None
		self._last_antenna = None
	
	def __del__(self):
		self.destroy()
	
	def destroy(self, error=None):
		pass
	
	def create(self, address=None, decim_rate=0, which=None, subdev_spec="", sample_rate=None):
		if self._uhd_usrp_source is not None:	# Not supporting re-creation
			return True
		
		if address is None:
			address = self._address
		if (address is None):	# or (isinstance(address, str) and address == "")
			address = _default_address
		
		if decim_rate == 0:
			decim_rate = self._decim_rate
		if decim_rate == 0:
			#raise Exception, "Decimation rate required"
			decim_rate = 256
		
		if which is None:
			which = self._which
		
		self._last_address = address
		self._last_which = which
		self._last_subdev_spec = subdev_spec
		
		self.destroy()
		
		# FIXME: 'which'
		
		if ((subdev_spec is not None) and (not isinstance(subdev_spec, str))) or (isinstance(subdev_spec, str) and (subdev_spec != "")):
			if isinstance(subdev_spec, str) == False:
				if isinstance(subdev_spec, tuple):
					if len(subdev_spec) > 1:
						subdev_spec = "%s:%s" % (chr(ord('A') + subdev_spec[0]), subdev_spec[1])
					else:
						subdev_spec = chr(ord('A') + subdev_spec[0])
				else:
					raise Exception, "Unknown sub-device specification: " + str(subdev_spec)
		
		self._uhd_usrp_source = uhd.usrp_source(
			device_addr=address,
			stream_args=uhd.stream_args(
				cpu_format="fc32",
				channels=range(1),
			),
		)
		
		if subdev_spec is not None:
			self._uhd_usrp_source.set_subdev_spec(subdev_spec, 0)
		
		try:
			info = self._uhd_usrp_source.get_usrp_info(0)
			self._name = info.get("mboard_id")
			serial = info.get("mboard_serial")
			if serial != "":
				self._name += (" (%s)" % (serial))
		except:
			pass
		
		_gr = self._uhd_usrp_source.get_gain_range(0)
		self._gain_range = (_gr.start(), _gr.stop())
		self._gain_step = _gr.step()
		
		self.connect(self._uhd_usrp_source, self)
		
		self._created = True
		
		if sample_rate is not None:
			self._uhd_usrp_source.set_samp_rate(sample_rate)
		else:
			if self.set_decim_rate(decim_rate) == False:
				raise Exception, "Invalid decimation: %s (sample rate: %s)" % (decim_rate, sample_rate)
		
		#if self._last_antenna is not None:
		#	self.select_rx_antenna(self._last_antenna)
		#if self._last_freq is not None:
		#	self.set_freq(self._last_freq)
		#if self._last_gain is not None:
		#	self.set_gain(self._last_gain)
	
	#def __repr__(self):
	#	pass
	
	def __str__(self):
		return self.name()
	
	def set_freq(self, freq):
		self._last_freq = freq
		self._tune_result = self._uhd_usrp_source.set_center_freq(freq, 0)
		#print "[UHD]", freq, "=", self._tune_result
		return self._tune_result	# usrp.usrp_tune_result
	
	def tune(self, unit, subdev, freq):
		return self.set_freq(freq)
	
	def adc_freq(self):
		return self._adc_freq
	
	def decim_rate(self):
		return self._decim_rate
	
	def set_mux(self, mux):
		pass
	
	def pick_subdev(self, opts):
		return ""	# subdev_spec
	
	def determine_rx_mux_value(self, subdev_spec, subdev_spec_=None):
		return 0	# Passed to set_mux
	
	def selected_subdev(self, subdev_spec):
		if (self._created == False):
			self.create(subdev_spec=subdev_spec)
		return self
	
	def set_decim_rate(self, decim_rate):
		self._decim_rate = decim_rate
		if self._uhd_usrp_source is None:
			return True
		sample_rate = self.adc_freq() / decim_rate
		#print "[UHD] Setting sample rate:", sample_rate
		return self._uhd_usrp_source.set_samp_rate(sample_rate)

	def db(self, side_idx, subdev_idx):
		return self
	
	def converter_rate(self):
		return self.adc_freq()
	
	## Daughter-board #################
	
	def dbid(self):
		return 0
	
	## Sub-device #####################

	def gain_range(self):
		return self._gain_range
	
	def gain_min(self):
		return self._gain_range[0]
	
	def gain_max(self):
		return self._gain_range[1]
	
	def set_gain(self, gain):
		self._last_gain = gain
		return self._uhd_usrp_source.set_gain(gain, 0)
	
	def select_rx_antenna(self, antenna):
		self._last_antenna = antenna
		return self._uhd_usrp_source.set_antenna(antenna, 0)
	
	def name(self):
		return self._name
	
	def side_and_name(self):
		try:
			info = self._uhd_usrp_source.get_usrp_info(0)
			return "%s [%s]" % (self.name(), info.get("rx_subdev_name"))
		except:
			return self.name()
