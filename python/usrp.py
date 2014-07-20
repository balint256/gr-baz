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

from gnuradio import gr, gru, uhd, blocks

_prefs = gr.prefs()
_default_address = _prefs.get_string('legacy_usrp', 'address', '')

def determine_rx_mux_value(u, subdev_spec, subdev_spec_b=None):
	return 0

def selected_subdev(u, subdev_spec):	# Returns subdevice object
	return u.selected_subdev(subdev_spec)

def tune(u, unit, subdev, freq):
	return u.tune(unit, subdev, freq)

class tune_result:
	def __init__(self, baseband_freq=0, actual_rf_freq=0, dxc_freq=0, residual_freq=0, inverted=False):
		self.baseband_freq = baseband_freq
		self.actual_rf_freq = actual_rf_freq
		self.dxc_freq = dxc_freq
		self.residual_freq = residual_freq
		self.inverted = inverted

class usrp_tune_result(tune_result):
	def __init__(self, baseband=None, dxc=None, residual=None, **kwds):
		tune_result.__init__(self, **kwds)
		if baseband is not None:
			self.baseband_freq = self.baseband = baseband
		if dxc is not None:
			self.dxc_freq = self.dxc = dxc
		if residual is not None:
			self.residual_freq = self.residual = residual

def pick_subdev(u, candidates=[]):
    return u.pick_subdev(candidates)

def pick_tx_subdevice(u):
	return (0, 0)

def pick_rx_subdevice(u):
	return (0, 0)

class device(gr.hier_block2):
	"""
	Legacy USRP via UHD
	Assumes 64MHz clock in USRP 1
	'address' as None implies check config for default
	"""
	def __init__(self, address=None, which=0, decim_rate=0, nchan=1, adc_freq=64e6, defer_creation=True, scale=8192):	# FIXME: 'which', 'nchan'
		"""
		UHD USRP Source
		"""
		if self._args[1] == "fc32":
			format_size = gr.sizeof_gr_complex
		elif self._args[1] == "sc16":
			format_size = gr.sizeof_short * 2
		
		empty_io = gr.io_signature(0, 0, 0)
		format_io = gr.io_signature(1, 1, format_size)
		
		if self._args[0]:
			io = (format_io, empty_io)
		else:
			io = (empty_io, format_io)
		
		gr.hier_block2.__init__(self, "uhd_usrp_wrapper",
			io[0],
			io[1])
		
		self._decim_rate = decim_rate
		self._address = address
		self._which = which
		
		self._adc_freq = int(adc_freq)
		self._gain_range = (0, 1, 1)
		self._freq_range = (0, 0, 0)
		self._tune_result = None
		self._name = "(Legacy USRP)"
		self._serial = "(Legacy)"
		self._gain_step = 0
		self._created = False
		
		self._scale = scale
		
		self._last_address = address
		self._last_which = None
		self._last_subdev_spec = None
		
		self._reset_last_params()
		
		self._uhd_device = None
		
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
		if self._uhd_device is not None:	# Not supporting re-creation
			return True
		
		if address is None:
			address = self._address
		if (address is None):	# or (isinstance(address, str) and address == "")
			address = _default_address
		
		if isinstance(address, int):
			# FIXME: Check 'which'
			which = address
			address = ""
		
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
		
		stream_args = uhd.stream_args(
			cpu_format=self._args[1],
			channels=range(1),
		)
		
		if self._args[0]:
			self._uhd_device = uhd.usrp_sink(
				device_addr=address,
				stream_args=stream_args,
			)
		else:
			self._uhd_device = uhd.usrp_source(
				device_addr=address,
				stream_args=stream_args,
			)
		
		if ((subdev_spec is not None) and (not isinstance(subdev_spec, str))) or (isinstance(subdev_spec, str) and (subdev_spec != "")):
			if isinstance(subdev_spec, str) == False:
				if isinstance(subdev_spec, tuple):
					if len(subdev_spec) > 1:
						default_subdev_spec = self._uhd_device.get_subdev_spec()
						idx = default_subdev_spec.find(':')
						if idx > -1:
							try:
								int(default_subdev_spec[idx+1:])
							except:
								subdev_spec = "%s:%s" % (chr(ord('A') + subdev_spec[0]), chr(ord('A') + subdev_spec[1]))
						if isinstance(subdev_spec, str) == False:
							subdev_spec = "%s:%s" % (chr(ord('A') + subdev_spec[0]), subdev_spec[1])
					else:
						subdev_spec = chr(ord('A') + subdev_spec[0])
				else:
					raise Exception, "Unknown sub-device specification: " + str(subdev_spec)
		
		if subdev_spec is not None:
			self._uhd_device.set_subdev_spec(subdev_spec, 0)
		
		try:
			info = self._uhd_device.get_usrp_info(0)
			self._name = info.get("mboard_id")
			self._serial = info.get("mboard_serial")
			if self._serial != "":
				self._name += (" (%s)" % (self._serial))
		except:
			pass
		
		_gr = self._uhd_device.get_gain_range(0)
		self._gain_range = (_gr.start(), _gr.stop(), _gr.step())
		self._gain_step = _gr.step()
		
		external_port = self
		self._multiplier = None
		if self._args[1] != "sc16" and self._scale != 1.0:
			scale = self._scale
			if self._args[0]:
				scale = 1.0 / scale
			#print "Scaling by", self._scale
			try: self._multiplier = external_port = blocks.multiply_const_vcc((scale,))
			except: self._multiplier = external_port = gr.multiply_const_vcc((scale,))
			if self._args[0]:
				self.connect(self, self._multiplier)
			else:
				self.connect(self._multiplier, self)
		
		_fr = self._uhd_device.get_freq_range(0)
		self._freq_range = (_fr.start(), _fr.stop(), _fr.step())
		
		if self._args[0]:
			if self._args[1] == "sc16":
				try:
					self._s2v = blocks.stream_to_vector(gr.sizeof_short, 2)
				except:
					self._s2v = gr.stream_to_vector(gr.sizeof_short, 2)
				self.connect(external_port, self._s2v, self._uhd_device)
			else:
				self.connect(external_port, self._uhd_device)
		else:
			if self._args[1] == "sc16":
				try:
					self._v2s = blocks.vector_to_stream(gr.sizeof_short, 2)
				except:
					self._v2s = gr.vector_to_stream(gr.sizeof_short, 2)
				self.connect(self._uhd_device, self._v2s, external_port)
			else:
				self.connect(self._uhd_device, external_port)
		
		self._created = True
		
		if sample_rate is not None:
			self._uhd_device.set_samp_rate(sample_rate)
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
		self._tune_result = self._uhd_device.set_center_freq(freq, 0)
		#print "[UHD]", freq, "=", self._tune_result
		#return self._tune_result	# usrp.usrp_tune_result
		tr = tune_result(
			baseband_freq = self._tune_result.actual_rf_freq,
			dxc_freq = self._tune_result.actual_dsp_freq,
			residual_freq = (freq - self._tune_result.actual_rf_freq - self._tune_result.actual_dsp_freq))
		return tr
	
	def tune(self, unit, subdev, freq):
		return self.set_freq(freq)
	
	def adc_freq(self):
		return self._adc_freq
	
	def adc_rate(self):
		return self.adc_freq()
	
	def decim_rate(self):
		return self._decim_rate
	
	def set_mux(self, mux):
		pass
	
	def pick_subdev(self, candidates=[]):
		return ""	# subdev_spec (side, subdev)
	
	def pick_tx_subdevice(self):
		return (0, 0)
	
	def pick_rx_subdevice(self):
		return (0, 0)
	
	def determine_rx_mux_value(self, subdev_spec, subdev_spec_=None):
		return 0	# Passed to set_mux
	
	def selected_subdev(self, subdev_spec):
		if (self._created == False):
			self.create(subdev_spec=subdev_spec)
		return self
	
	def set_decim_rate(self, decim_rate):
		self._decim_rate = decim_rate
		if self._uhd_device is None:
			return True
		sample_rate = self.adc_freq() / decim_rate
		#print "[UHD] Setting sample rate:", sample_rate
		return self._uhd_device.set_samp_rate(sample_rate)

	def db(self, side_idx, subdev_idx):
		return self
	
	def converter_rate(self):
		return self.adc_freq()
	
	def fpga_master_clock_freq(self):
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
		return self._uhd_device.set_gain(gain, 0)
	
	def freq_range(self):
		return self._freq_range
	
	def select_rx_antenna(self, antenna):
		self._last_antenna = antenna
		return self._uhd_device.set_antenna(antenna, 0)
	
	def name(self):
		return self._name
	
	def serial_number(self):
		return self._serial
	
	def side_and_name(self):
		try:
			info = self._uhd_device.get_usrp_info(0)
			return "%s [%s]" % (self.name(), info.get("rx_subdev_name"))
		except:
			return self.name()

class source_c(device): _args = (False, "fc32")
class source_s(device): _args = (False, "sc16")
class sink_c(device): _args = (True, "fc32")
class sink_s(device): _args = (True, "sc16")
