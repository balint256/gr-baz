#!/usr/bin/env python

"""
http://wiki.spench.net/wiki/gr-baz
By Balint Seeber (http://spench.net/contact)
"""

from __future__ import with_statement

import time, os, sys, threading, thread
from string import split, join

from gnuradio import gr, gru, uhd

def pick_rx_subdevice(u):
	return 0

def determine_rx_mux_value(u, subdev_spec, subdev_spec_=None):
	return 0
	
def selected_subdev(u, subdev_spec):
	return u

class source_c(gr.hier_block2):
	"""
	Remote USRP via BorIP
	"""
	def __init__(self, address="", which=0, decim_rate=0):
		"""
		Remote USRP. Remember to call 'create'
		"""
		gr.hier_block2.__init__(self, "uhd_usrp_wrapper",
			gr.io_signature(0, 0, 0),
			gr.io_signature(1, 1, gr.sizeof_gr_complex))
		
		self._decim_rate = decim_rate
		self._address = address
		self._which = which
		
		self._adc_freq = int(64e6)
		#self._gain_range = (0, 1)
		#self._tune_result = None
		#self._name = "(Remote device)"
		#self._gain_step = 0
		self._created = False
		
		self._last_address = address	# None
		self._last_which = None
		self._last_subdev_spec = None
		
		self._reset_last_params()
		
		self.uhd_usrp_source = uhd.usrp_source(
			device_addr=address,
			stream_args=uhd.stream_args(
				cpu_format="fc32",
				channels=range(1),
			),
		)
		
		self.connect(self.uhd_usrp_source, self)
	
	def _reset_last_params(self):
		self._last_freq = None
		self._last_gain = None
		self._last_antenna = None
	
	def __del__(self):
		self.destroy()
	
	def destroy(self, error=None):
		pass
	
	def create(self, address=None, decim_rate=0, which=None, subdev_spec="", sample_rate=None):
		if address is None:
			address = self._address
		if (address is None) or (isinstance(address, str) and address == ""):
			address = _default_server_address
		if address == "":
			raise Exception, "Server address required"
		
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
		
		if reconnect is False:
			self._reset_last_params()
		
		self.destroy()
		
		self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		
		#print "Connecting to: " + str(address)
		
		port = _default_port
		if isinstance(address, str):
			parts = address.split(":")
			if len(parts) > 1:
				port = int(parts[1])
			address = (parts[0], port)
		
		if udp_port is None:
			udp_port = port
		
		if address[0] == "-":
			self._listen_only = True
			if (self._packet_size == 0) or (packet_size > 0):
				if packet_size == 0:
					packet_size = 4096 * 2 * 2	# FCD testing: 9216
				self._packet_size = packet_size
			print "BorIP client only listening on port %d (MTU: %d)" % (udp_port, self._packet_size)
		else:
			self._listen_only = False
			
			reconnect_attempts = self._reconnect_attempts
			
			while True:
				try:
					self.s.connect(address)
					#print "BorIP client connected to: %s" % (str(address))
				except socket.error, (e, msg):
					if (e == 111) and (reconnect is False) and ((self._reconnect_attempts < 0) or ((self._reconnect_attempts > 0) and (reconnect_attempts > 0))):	# Connection refused
						error_str = "Connection refused, trying again"
						if (self._reconnect_attempts > 0):
							error_str = error_str + (" (%s attempts remaining)" % (reconnect_attempts))
						print error_str
						reconnect_attempts = reconnect_attempts - 1
						
						try:
							time.sleep(_reconnect_interval)
							continue
						except KeyboardInterrupt:
							raise socket.error, (e, msg)
							
					print "Failed to connect to server: %s %s" % (e, msg)
					raise socket.error, (e, msg)
				break
			
			self._reconnect_attempts_to_go = self._reconnect_attempts
			
			(cmd, res, data) = self._recv()
			if cmd != "DEVICE":
				if cmd == "BUSY":
					raise Exception, "Server is busy"
				else:
					raise Exception, "Unexpected greeting: " + cmd
				
			#print "Server ready"
			
			if (res == "-") or (which is not None):
				hint = str(which)
				if ((subdev_spec is not None) and (not isinstance(subdev_spec, str))) or (isinstance(subdev_spec, str) and (subdev_spec != "")):
					if isinstance(subdev_spec, str) == False:
						if isinstance(subdev_spec, tuple):
							if len(subdev_spec) > 1:
								subdev_spec = "%s:%s" % (subdev_spec[0], subdev_spec[1])
							else:
								subdev_spec = str(subdev_spec[0])
						else:
							raise Exception, "Unknown sub-device specification: " + str(subdev_spec)
					hint += " " + subdev_spec
				self._send("DEVICE", hint)
			
			self._created = True
			#print "Created remote device: %s" % (self._name)
			
			#self._send("HEADER", "OFF")	# Enhanced udp_source
			
			if sample_rate is not None:
				self._send_and_wait_for_ok("RATE", sample_rate)
			else:
				#sample_rate = self.adc_freq() / decim_rate
				if self.set_decim_rate(decim_rate) == False:
					raise Exception, "Invalid decimation: %s (sample rate: %s)" % (decim_rate, sample_rate)
		
		if self.udp_source is None:
			assert self.vec2stream is None and self.ishort2complex is None
			udp_interface = "0.0.0.0"	# MAGIC
			self.udp_source = gr.udp_source(gr.sizeof_short * 2, udp_interface, udp_port, self._packet_size, True, True, True)
			#print "--> UDP Source listening on port:", udp_port, "interface:", udp_interface, "MTU:", self._packet_size
			self.vec2stream = gr.vector_to_stream(gr.sizeof_short * 1, 2)
			self.ishort2complex = gr.interleaved_short_to_complex()
		
			self.connect(self.udp_source, self.vec2stream, self.ishort2complex, self)
		else:
			assert self.vec2stream is not None and self.ishort2complex is not None
		
		if self._listen_only == False:
			if self._last_antenna is not None:
				self.select_rx_antenna(self._last_antenna)
			if self._last_freq is not None:
				self.set_freq(self._last_freq)
			if self._last_gain is not None:
				self.set_gain(self._last_gain)
			
			self._send_and_wait_for_ok("GO")	# Will STOP on disconnect
			
			self._keepalive_thread = keepalive_thread(self)
			self._keepalive_thread.start()
	
	#def __repr__(self):
	#	pass
	
	def set_freq(self, freq):
		self._last_freq = freq
		tr = self.uhd_usrp_source.set_center_freq(freq, 0)
		#print freq
		print tr
		return tr	# usrp.usrp_tune_result
	
	def __str__(self):
		return self.name()
	
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
		sample_rate = self.adc_freq() / decim_rate
		#print "UHD: Setting sample rate:", sample_rate
		return self.uhd_usrp_source.set_samp_rate(sample_rate)

	def db(self, side_idx, subdev_idx):
		return self
	
	def converter_rate(self):
		return self.adc_freq()
	
	## Daughter-board #################
	
	def dbid(self):
		return 0
	
	## Sub-device #####################

	def gain_range(self):
		_gr = self.uhd_usrp_source.get_gain_range(0)
		return [_gr.start(), _gr.stop()]
	
	def gain_min(self):
		return self.gain_range()[0]
	
	def gain_max(self):
		return self.gain_range()[1]
	
	def set_gain(self, gain):
		self._last_gain = gain
		return self.uhd_usrp_source.set_gain(gain, 0)
	
	def select_rx_antenna(self, antenna):
		self._last_antenna = antenna
		return self.uhd_usrp_source.set_antenna(antenna, 0)

	def name(self):
		return self._name
	
	def side_and_name(self):
		return self.name()

###########################################################

#if hasattr(usrp, '_orig_source_c') == False:
#	usrp._orig_source_c = usrp.source_c
#	
#	def _borip_source_c(which=0, decim_rate=256, nchan=None):
#		#global _default_server_address
#		if _default_server_address == "":
#			return usrp._orig_source_c(which=which, decim_rate=decim_rate)
#		try:
#			return usrp._orig_source_c(which=which, decim_rate=decim_rate)
#		except:
#			return remote_usrp(None, which, decim_rate)
	
#	usrp.source_c = _borip_source_c
