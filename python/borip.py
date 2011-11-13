#!/usr/bin/env python

from __future__ import with_statement

import time, os, sys, threading, thread
from string import split, join
#from optparse import OptionParser
import socket
import threading
#import SocketServer

from gnuradio import gr, gru

if sys.modules.has_key('gnuradio.usrp'):
	usrp =  sys.modules['gnuradio.usrp']
else:
	from gnuradio import usrp

_prefs = gr.prefs()
_default_server_address = _prefs.get_string('borip', 'server', '')

class remote_usrp(gr.hier_block2):
	"""
	Remote USRP via BorIP
	"""
	def __init__(self, address, which=0, decim_rate=0):
		"""
		Remote USRP. Remember to call 'create'
		"""
		gr.hier_block2.__init__(self, "remote_usrp",
			gr.io_signature(0, 0, 0),
			gr.io_signature(1, 1, gr.sizeof_gr_complex))
		
		self._decim_rate = decim_rate
		self._address = address
		self._which = which
		
		self.s = None
		self._adc_freq = 64e6
		self._buffer = ""
		self._gain_range = (0, 1)
		#self._tune_result = None
		self._name = "(Remote device)"
		self._gain_step = 0
		self._packet_size = 0
		self._created = False
	
	def __del__(self):
		self.destroy()
	
	def _send_raw(self, command, data=None):
		if data is not None:
			command += " " + str(data)
		try:
			self.s.send(command.strip() + "\n")
		except socket.error, (e, msg):
			if (e != 32): # Broken pipe
				self.destroy()
				raise socket.error, (e, msg)
	
	def _send(self, command, data=None):
		self._send_raw(command, data)
		return self._recv()
	
	def _send_and_wait_for_ok(self, command, data=None):
		(cmd, res, data) = self._send(command, data)
		if (cmd != command):
			raise Exception, "Receive command %s != %s" % (cmd, command)
		if res != "OK":
			raise Exception, "Expecting OK, received %s" % (res)
	
	def _recv(self):
		while True:
			try:
				response = self.s.recv(1024)
			except socket.error, (e, msg):
				#if e != 104:    # Connection reset by peer
				#	pass
				self.destroy()
				raise socket.error, (e, msg)
			
			#if len(response) == 0:
			#	#return (None, None, None)
			#	self.destory()
			#	raise Exception, "Disconnected"
			
			self._buffer += response
			lines = self._buffer.splitlines(True)
			
			#for line in lines:
			if len(lines) > 0:	# Should only be one line at a time
				line = lines[0]
				
				if line[-1] != '\n':
					self._buffer = line
					continue
				
				line = line.strip()
				
				#print "Received: " + line
				
				cmd = line
				res = None
				data = None
				
				idx = cmd.find(" ")
				if idx > -1:
					res = (cmd[idx + 1:]).strip()
					cmd = (cmd[0:idx]).upper()
					
					if cmd != "DEVICE":
						idx = res.find(" ")
						if idx > -1:
							data = (res[idx + 1:]).strip()
							res = (res[0:idx])
						res = res.upper()
				elif cmd.upper() == "BUSY":
					pass
				else:
					raise Exception, "Response without result: " + line
				
				if res == "FAIL":
					#raise Exception, "Failure: " + line
					pass
				elif res == "DEVICE":
					#raise Exception, "Need to create device first"
					pass
				elif cmd == "DEVICE":
					if res == "-":
						if data is not None:
							raise Exception, "Failed to create device: \"%s\"" % data
					else:
						parts = res.split("|")
						try:
							if parts[0] == "":
								self._name = "(No name)"
							else:
								self._name = parts[0]
							self._gain_range = (float(parts[1]), float(parts[2]))
							self._gain_step = float(parts[3])
							self._adc_freq = int(float(parts[4]))
							self._packet_size = int(parts[5]) * 2 * 2
							# FIXME: Antennas
						except:
							raise Exception, "Malformed device response: " + res
				elif cmd == "RATE":
					pass
				elif cmd == "GAIN":
					pass
				elif cmd == "ANTENNA":
					pass
				elif cmd == "FREQ":
					if res == "OK":
						parts = data.split(" ")
						data = usrp.usrp_tune_result(baseband=float(parts[1]), dxc=float(parts[3]), residual=0)
					pass
				#elif cmd == "BUSY":
				#	raise "Client is busy"
				#elif cmd == "GO":
				#	pass
				#else:
				#	print "Unhandled response: " + line
				
				self._buffer = "".join(lines[1:])
				
				return (cmd, res, data)
			else:
				raise Exception, "No data received"
	
	def destroy(self):
		if self.s is not None:
			self.s.close()
			self.s = None
		
		self._buffer = ""
		self._created = False
	
	def create(self, address=None, decim_rate=0, which=None, subdev_spec="", udp_port=None, sample_rate=None):
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
		
		self.destroy()
		
		self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		
		#print "Connecting to: " + str(address)
		
		port = 28888
		if isinstance(address, str):
			parts = address.split(":")
			if len(parts) > 1:
				port = int(parts[1])
			address = (parts[0], port)
		
		if udp_port is None:
			udp_port = port
		
		try:
			self.s.connect(address)
		except socket.error, (e, msg):
			print "Failed to connect to server: %s %s" % (e, msg)
			raise socket.error, (e, msg)
		
		(cmd, res, data) = self._recv()
		if cmd != "DEVICE":
			if cmd == "BUSY":
				raise Exception, "Server is busy"
			else:
				raise Exception, "Unexpected greeting: " + cmd
		
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
		
		#self._send("HEADER", "OFF")	# Enhanced udp_source
		
		if sample_rate is not None:
			self._send_and_wait_for_ok("RATE", sample_rate)
		else:
			#sample_rate = self.adc_freq() / decim_rate
			if self.set_decim_rate(decim_rate) == False:
				raise Exception, "Invalid decimation: %s (sample rate: %s)" % (decim_rate, sample_rate)
		
		udp_interface = "0.0.0.0"	# MAGIC
		self.udp_source = gr.udp_source(gr.sizeof_short * 2, udp_interface, udp_port, self._packet_size, True, True, True)
		#print "--> UDP Source listening on port:", udp_port, "interface:", udp_interface, "MTU:", packet_mtu
		self.vec2stream = gr.vector_to_stream(gr.sizeof_short * 1, 2)
		self.ishort2complex = gr.interleaved_short_to_complex()
		
		self.connect(self.udp_source, self.vec2stream, self.ishort2complex, self)
		
		self._send_and_wait_for_ok("GO")	# Will STOP on disconnect
	
	#def __repr__(self):
	#	pass
	
	def set_freq(self, freq):
		(cmd, res, data) = self._send("FREQ", freq)
		return data	# usrp.usrp_tune_result
	
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
		if self.s is None:
			self._decim_rate = decim_rate
			return
		sample_rate = self.adc_freq() / decim_rate
		(cmd, res, data) = self._send("RATE", sample_rate)
		if (res == "OK"):
			self._decim_rate = decim_rate
			return True
		return False

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
		(cmd, res, data) = self._send("GAIN", gain)
		return (res == "OK")
	
	def select_rx_antenna(self, antenna):
		(cmd, res, data) = self._send("ANTENNA", antenna)
		return (res == "OK")

	def name(self):
		return self._name
	
	def side_and_name(self):
		return self.name()

###########################################################

#if ('usrp' in locals()):
if hasattr(usrp, '_orig_source_c') == False:
	usrp._orig_source_c = usrp.source_c
	
	def _borip_source_c(which, decim_rate=0, nchan=None):
		#global _default_server_address
		if _default_server_address == "":
			return usrp._orig_source_c(which=which, decim_rate=decim_rate)
		try:
			return usrp._orig_source_c(which=which, decim_rate=decim_rate)
		except:
			return remote_usrp(None, which, decim_rate)
	
	usrp.source_c = _borip_source_c
	
	###################
	
	#if hasattr(usrp, '_orig_pick_subdev') == False:
	#	usrp._orig_pick_subdev = usrp.pick_subdev
	#	
	#	def _borip_pick_subdev(u, opts):
	#		if isinstance(u, remote_usrp):
	#			return ""	# subdev_spec
	#		return usrp._orig_pick_subdev(u, opts)
	#	
	#	usrp.pick_subdev = _borip_pick_subdev
	
	###################
	
	#if hasattr(usrp, '_orig_determine_rx_mux_value') == False:
	#	usrp._orig_determine_rx_mux_value = usrp.determine_rx_mux_value
	#	
	#	def _borip_determine_rx_mux_value(u, subdev_spec):
	#		if isinstance(u, remote_usrp):
	#			return 0	# Passed to set_mux
	#		return usrp._orig_determine_rx_mux_value(u, subdev_spec)
	#	
	#	usrp.determine_rx_mux_value = _borip_determine_rx_mux_value
	
	###################
	
	#if hasattr(usrp, '_orig_selected_subdev') == False:
	#	usrp._orig_selected_subdev = usrp.selected_subdev
	#	
	#	def _borip_selected_subdev(u, subdev_spec):
	#		if isinstance(u, remote_usrp):
	#			if (u._created == False):
	#				u.create(subdev_spec=subdev_spec)
	#			return u
	#		return usrp._orig_selected_subdev(u, subdev_spec)
	#	
	#	usrp.selected_subdev = _borip_selected_subdev
