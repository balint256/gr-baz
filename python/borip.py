#!/usr/bin/env python

"""
BorIP client for GNU Radio.
Enables access to a remote USRP via BorIP server through a LAN.
Uses gr_udp_source with modifications to enable BorIP packet support.
Hooks usrp.source_c so BorIP will automatically attempt to connect to a remote server if a local USRP is not present.
To specify a default server, add 'server=<server>' to the '[borip]' section of your ~/.gnuradio/config.conf (see other settings below).
More information regarding operational modes, seamless integration and settings: http://wiki.spench.net/wiki/gr-baz#borip
BorIP protocol specification: http://spench.net/r/BorIP
By Balint Seeber (http://spench.net/contact)
"""

# NOTES:
# Wait-for-OK will fail if socket fails, despite 'destroy' reconnect recovery
# GRC generated doesn't honour external config 'reconnect_attempts'

from __future__ import with_statement

import time, os, sys, threading, thread
from string import split, join
#from optparse import OptionParser
import socket
import threading
#import SocketServer

from gnuradio import gr, gru, blocks

import baz

usrp = None
if sys.modules.has_key('gnuradio.usrp'):
	usrp =  sys.modules['gnuradio.usrp']
else:
	try:
		from gnuradio import usrp	# Will work if libusrp is installed, or symlink exists to local usrp.py
	except:
		import usrp

_default_port = 28888
_reconnect_interval = 5
_keepalive_interval = 5
_verbose = False
_reconnect_attempts = 0

_prefs = gr.prefs()
_default_server_address = _prefs.get_string('borip', 'server', '')
try:
	_reconnect_attempts = int(_prefs.get_string('borip', 'reconnect_attempts', str(_reconnect_attempts)))
except:
	pass
try:
	_reconnect_interval = int(_prefs.get_string('borip', 'reconnect_interval', str(_reconnect_interval)))
except:
	pass
try:
	_verbose = not ((_prefs.get_string('borip', 'verbose', str(_verbose))).strip().lower() in ['false', 'f', 'n', '0', ''])
except:
	pass
try:
	_default_port = int(_prefs.get_string('borip', 'default_port', str(_default_port)))
except:
	pass
try:
	_keepalive_interval = int(_prefs.get_string('borip', 'keepalive_interval', str(_keepalive_interval)))
except:
	pass

class keepalive_thread(threading.Thread):
	def __init__(self, rd, interval=_keepalive_interval, **kwds):
		threading.Thread.__init__ (self, **kwds)
		self.setDaemon(1)
		self.rd = rd
		self.interval = interval
	def start(self):
		self.active = True
		threading.Thread.start(self)
	def stop(self):
		self.active = False
	def run(self):
		while self.active:
			time.sleep(self.interval)
			with self.rd._socket_lock:
				if self.active == False:
					break
				try:
					(cmd, res, data) = self.rd._send("PING")
					#print "Keepalive: %s -> %s" % (cmd, res)	# Ignore response
				except socket.error, (e, msg):
					self.active = False
					break
		#print "Keepalive thread exiting"

class remote_usrp(gr.hier_block2):
	"""
	Remote USRP via BorIP
	"""
	def __init__(self, address, which=0, decim_rate=0, packet_size=0, reconnect_attempts=None, format="fc32"):
		"""
		Remote USRP. Remember to call 'create'
		"""
		valid_formats = {'fc32': gr.sizeof_gr_complex, 'sc16': (gr.sizeof_short * 2)}
		if format not in valid_formats.keys():
			raise Exception("Invalid output format: '%s' (must be one of: %s)" % (str(format), ", ".join(valid_formats)))
		
		gr.hier_block2.__init__(self, "remote_usrp",
			gr.io_signature(0, 0, 0),
			gr.io_signature(1, 1, valid_formats[format]))
		
		self._decim_rate = decim_rate
		self._address = address
		self._which = which
		self._format = format
		
		self.s = None
		self._adc_freq = int(64e6)
		self._buffer = ""
		self._gain_range = (0, 1)
		#self._tune_result = None
		self._name = "(Remote device)"
		self._gain_step = 0
		self._packet_size = packet_size
		self._created = False
		self._listen_only = False
		self._socket_lock = threading.RLock()			# Re-entrant!
		self._keepalive_thread = None
		if reconnect_attempts is not None:
			self._reconnect_attempts = reconnect_attempts
		else:
			self._reconnect_attempts = _reconnect_attempts
		self._reconnect_attempts_to_go = self._reconnect_attempts
		
		self.udp_source = None
		self.vec2stream = None
		self.ishort2complex = None
		self.multiply_const = None
		
		self._last_address = None
		self._last_which = None
		self._last_subdev_spec = None
		
		self._reset_last_params()
	
	def _reset_last_params(self):
		self._last_freq = None
		self._last_gain = None
		self._last_antenna = None
	
	def __del__(self):
		self.destroy()
	
	def _send_raw(self, command, data=None):
		if self._listen_only:
			return False
		if data is not None:
			command += " " + str(data)
		with self._socket_lock:
			if self.s is None:
				return False
			try:
				command = command.strip()
				if _verbose:
					print "-> " + command
				self.s.send(command + "\n")
			except socket.error, (e, msg):
				#if (e != 32): # Broken pipe
				if self.destroy(e) is not True:
					raise socket.error, (e, msg)
			return True
	
	def _send(self, command, data=None):
		with self._socket_lock:
			if self._send_raw(command, data) == False:
				return (command, None, None)
			return self._recv()
	
	def _send_and_wait_for_ok(self, command, data=None):
		(cmd, res, data) = self._send(command, data)
		if (cmd != command):
			raise Exception, "Receive command %s != %s" % (cmd, command)
		if res != "OK":
			raise Exception, "Expecting OK, received %s" % (res)
	
	def _recv(self):
		with self._socket_lock:
			if self.s is None:
				return (command, None, None)
			while True:
				try:
					response = self.s.recv(1024)
				except socket.error, (e, msg):
					#if e != 104:    # Connection reset by peer
					#	pass
					if self.destroy(e) is not True:
						raise socket.error, (e, msg)
					else:
						return (None, None, None)
				
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
					
					if _verbose:
						print "<- " + line
					
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
					if self.destroy(32) is not True:	# Broken pipe
						raise Exception, "No data received (server connection was closed)"
						#pass	# Will signal
					else:
						return (None, None, None)
	
	def destroy(self, error=None):
		if self.s is not None:
			self.s.close()
			self.s = None
		
		self._buffer = ""
		self._created = False
		
		if self._keepalive_thread is not None:
			self._keepalive_thread.stop()
			self._keepalive_thread = None
		
		if error is not None:
			#print "Destroy: %s" % (error)
			assert self._listen_only == False
			while True:
				if (self._reconnect_attempts_to_go > 0) or (self._reconnect_attempts_to_go < 0):
					if (self._reconnect_attempts_to_go > 0):
						print "Reconnect attempts remaining: %s" % (self._reconnect_attempts_to_go)
					
					self._reconnect_attempts_to_go = self._reconnect_attempts_to_go - 1
					
					try:
						self.create(address=self._last_address, which=self._last_which, subdev_spec=self._last_subdev_spec, reconnect=True)
						return True
					except:
						try:
							time.sleep(_reconnect_interval)
							continue
						except KeyboardInterrupt:
							pass	# Fall through to EOS
				#else:
				self.udp_source.signal_eos()
				return False
		
		return None
	
	def create(self, address=None, decim_rate=0, which=None, subdev_spec="", udp_port=None, sample_rate=None, packet_size=0, reconnect=False):
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
				if len(hint) == 0:
					hint = "-"	# Create default
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
			self.udp_source = baz.udp_source(gr.sizeof_short * 2, udp_interface, udp_port, self._packet_size, True, True, True)
			#print "--> UDP Source listening on port:", udp_port, "interface:", udp_interface, "MTU:", self._packet_size
			if self._format == "fc32":
				try: self.vec2stream = blocks.vector_to_stream(gr.sizeof_short * 1, 2)
				except: self.vec2stream = gr.vector_to_stream(gr.sizeof_short * 1, 2)
				try: self.ishort2complex = blocks.interleaved_short_to_complex()	# FIXME: Use vector mode and skip 'vector_to_stream'
				except: self.ishort2complex = gr.interleaved_short_to_complex()
				mul_factor = 1./2**15
				try: self.multiply_const = blocks.multiply_const_cc(mul_factor)
				except: self.multiply_const = gr.multiply_const_cc(mul_factor)
			
				self.connect(self.udp_source, self.vec2stream, self.ishort2complex, self.multiply_const, self)
			elif self._format == "sc16":
				self.connect(self.udp_source, self)
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
		self._last_gain = gain
		(cmd, res, data) = self._send("GAIN", gain)
		return (res == "OK")
	
	def select_rx_antenna(self, antenna):
		self._last_antenna = antenna
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
	
	def _borip_source_c(which=0, decim_rate=256, nchan=None):
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
