#!/usr/bin/env python

"""
Automatically try each combination of FEC parameters until the correct one is found.
The deFEC routine is inside decode_ccsds_27_fb (NASA Voyager, k=7, 1/2 rate).
Parameters are controller by auto_fec_xform.
Keep an eye on the console to watch the progress of the parameter search.
The auto_fec block creation arguments control its behaviour.
Ordinarily leave sample_rate at 0 to have the other durations/periods interpreted as sample counts.
If the sample rate is specified, you can also engage the internal throttle if playing back from a file.
Part of gr-baz. More info: http://wiki.spench.net/wiki/gr-baz
By Balint Seeber (http://spench.net/contact)
"""

from __future__ import with_statement

import threading, math, time
import wx
import numpy

from gnuradio import gr, blocks, fec, filter, analog
from grc_gnuradio import blks2 as grc_blks2
import baz

_puncture_matrices = [
		('1/2', [1,1], (1, 2)),
		('2/3', [1,1,0,1], (2, 3)),
		('3/4', [1,1,0,1,1,0], (3, 4)),
		('5/6', [1,1,0,1,1,0,0,1,1,0], (5, 6)),
		('7/8', [1,1,0,1,0,1,0,1,1,0,0,1,1,0], (7, 8)),
		('2/3*', [1,1,1,0], (2, 3)),
		('3/4*', [1,1,1,0,0,1], (3, 4)),
		('5/6*', [1,1,1,0,0,1,1,0,0,1], (5, 6)),
		('7/8*', [1,1,1,0,1,0,1,0,0,1,1,0,0,1], (7, 8))
	]

_phase_multiplication = [
		('0', 1),
		('90', 1j),
		# Others are just inverse of former - inverted stream can be fixed manually
		#('180', -1),
		#('270', -1j)
	]

class auto_fec_xform():
	#CHANGE_EVERYTHING = -1
	#CHANGE_NOTHING = 0
	CHANGE_ROTATION = 1
	CHANGE_CONJUGATION = 2
	CHANGE_INVERSION = 3
	CHANGE_PUNCTURE_DELAY = 4
	CHANGE_VITERBI_DELAY = 5
	CHANGE_VITERBI_SWAP = 6
	_clonable = [
			'conjugate',
			'rotation',
			'invert',
			'puncture_delay',
			'viterbi_delay',
			'viterbi_delay',
			'viterbi_swap'
		]
	def __init__(self):
		self.conjugate = False
		self.rotation = 0
		self.invert = False
		self.puncture_delay = 0
		self.viterbi_delay = False
		self.viterbi_swap = False
	def copy(self):
		clone = auto_fec_xform()
		#for k in auto_fec_xform._clonable:
		#	clone[k] = self[k]
		clone.conjugate = self.conjugate
		clone.rotation = self.rotation
		clone.invert = self.invert
		clone.puncture_delay = self.puncture_delay
		clone.viterbi_delay = self.viterbi_delay
		clone.viterbi_swap = self.viterbi_swap
		return clone
	def get_conjugation_index(self):
		if self.conjugate:
			return 0
		return 1
	def get_rotation(self):
		return _phase_multiplication[self.rotation][1]
	def get_inversion(self):
		if self.invert:
			return -1
		return 1
	def get_puncture_delay(self):
		return self.puncture_delay
	def get_viterbi_delay(self):
		if self.viterbi_delay:
			return 1
		return 0
	def get_viterbi_swap(self):
		return self.viterbi_swap
	def next(self, ref, fec_rate, psk_order=4):
		changes = []
		
		changes += [auto_fec_xform.CHANGE_ROTATION]
		# FIXME: Handle arbitrary PSK order
		self.rotation = (self.rotation + 1) % len(_phase_multiplication)	# Not doing inversion as this takes care of it
		if self.rotation != ref.rotation:
			return (True, changes)
		
		changes += [auto_fec_xform.CHANGE_CONJUGATION]
		self.conjugate = not self.conjugate
		if self.conjugate != ref.conjugate:
			return (True, changes)
		
		# Skipping inversion (handled by constellation rotation)
		
		changes += [auto_fec_xform.CHANGE_VITERBI_DELAY]
		self.viterbi_delay = not self.viterbi_delay
		if self.viterbi_delay != ref.viterbi_delay:
			return (True, changes)
		
		changes += [auto_fec_xform.CHANGE_VITERBI_SWAP]
		self.viterbi_swap = not self.viterbi_swap
		if self.viterbi_swap != ref.viterbi_swap:
			return (True, changes)
		
		changes += [auto_fec_xform.CHANGE_PUNCTURE_DELAY]
		self.puncture_delay = (self.puncture_delay + 1) % (2 * fec_rate[0])
		if self.puncture_delay != ref.puncture_delay:
			return (True, changes)
		
		return (False, changes)	# Back to reference point (if FEC unlocked, try next rate)

class auto_fec_input_watcher (threading.Thread):
	def __init__ (self, auto_fec_block, **kwds):
		threading.Thread.__init__ (self, **kwds)
		self.setDaemon(1)
		
		self.afb = auto_fec_block
		
		self.keep_running = True
		#self.skip = 0
		self.total_samples = 0
		self.last_sample_count_report = 0
		self.last_msg_time = None
		
		self.lock = threading.Lock()
		
		self.sample_rate = self.afb.sample_rate
		self.set_sample_rate(self.afb.sample_rate)
		self.ber_duration = self.afb.ber_duration
		self.settling_period = self.afb.settling_period
		self.pre_lock_duration = self.afb.pre_lock_duration
		
		###############################
		#self.excess_ber_count = 0
		#self.samples_since_excess = 0
		
		#self.fec_found = False
		#self.puncture_matrix = 0
		#self.xform_search = None
		#self.xform_lock = auto_fec_xform()
		self.reset()
		###############################
		
		self.skip_samples = self.settling_period	# + self.ber_duration
	def set_sample_rate(self, rate):
		#print "Adjusting durations with new sample rate:", rate
		if rate is None or rate <= 0:
			#rate = 1000
			return
		with self.lock:
			self.sample_rate = rate
			#self.ber_duration = int((self.afb.ber_duration / 1000.0) * rate)
			#self.settling_period = int((self.afb.settling_period / 1000.0) * rate)
			#self.pre_lock_duration = int((self.afb.pre_lock_duration / 1000.0) * rate)
			#print "\tber_duration:\t\t", self.ber_duration
			#print "\tsettling_period:\t", self.settling_period
			#print "\tpre_lock_duration:\t", self.pre_lock_duration
			#print ""
	def reset(self):
		self.excess_ber_count = 0
		self.samples_since_excess = 0
		self.excess_ber_sum = 0
		
		self.fec_found = False
		self.puncture_matrix = 0
		self.xform_search = None
		self.xform_lock = auto_fec_xform()
	def set_reset(self):
		with self.lock:
			print "==> Resetting..."
			self.reset()
			self.afb.update_matrix(_puncture_matrices[self.puncture_matrix][1])
			self.afb.update_xform(self.xform_lock)
			self.afb.update_lock(0)
			#print "    Reset."
	def set_puncture_matrix(self, matrix):
		# Not applying, just trigger another search
		self.set_reset()
	def run (self):
		print "Auto-FEC thread started:", self.getName()
		print "Skipping initial samples while MPSK receiver locks:", self.skip_samples
		print ""
		
		# Already applied in CTOR
		#print "Applying default FEC parameters..."
		#self.afb.update_matrix(_puncture_matrices[self.puncture_matrix][1])
		#self.afb.update_xform(self.xform_lock)
		#print "Completed applying default FEC parameters."
		#print ""
		
		while (self.keep_running):
			msg = self.afb.msg_q.delete_head()	# blocking read of message queue
			nchan = int(msg.arg1())				# number of channels of data in msg
			nsamples = int(msg.arg2())			# number of samples in each channel
			
			if self.last_msg_time is None:
				self.last_msg_time = time.time()
			
			self.total_samples += nsamples
			
			with self.lock:
				if self.sample_rate > 0 and (self.total_samples - self.last_sample_count_report) >= self.sample_rate:
					diff = self.total_samples - self.last_sample_count_report - self.sample_rate
					print "==> Received total samples:", self.total_samples, "diff:", diff
					time_now = time.time()
					time_diff = time_now - self.last_msg_time
					print "==> Time diff:", time_diff
					self.last_msg_time = time_now
					self.last_sample_count_report = self.total_samples - diff
			
			if self.skip_samples >= nsamples:
				self.skip_samples -= nsamples
				continue
			
			start = self.skip_samples * gr.sizeof_float	#max(0, self.skip_samples - (self.skip_samples % gr.sizeof_float))
			self.skip_samples = 0
			#if start > 0:
			#	print "Starting sample processing at byte index:", start, "total samples received:", self.total_samples
			
			data = msg.to_string()
			assert nsamples == (len(data) / 4)
			data = data[start:]
			samples = numpy.fromstring(data, numpy.float32)
			
			with self.lock:
				#print "Processing samples:", len(samples)
				
				excess_ber = False
				for x in samples:
					if x >= self.afb.ber_threshold:
						self.excess_ber_count += 1
						self.excess_ber_sum += x
						if self.excess_ber_count >= self.ber_duration:
							excess_ber = True
							#self.excess_ber_count = 0
							break
					else:
						if self.excess_ber_count > 0:
							if self.fec_found:
								print "Excess BER count was:", self.excess_ber_count
							self.excess_ber_count = 0
							self.excess_ber_sum = 0
				
				if excess_ber:
					excess_ber_ave = self.excess_ber_sum / self.excess_ber_count
					self.excess_ber_sum = 0
					self.excess_ber_count = 0
					print "Reached excess BER limit:", excess_ber_ave, ", locked:", self.fec_found, ", current puncture matrix:", self.puncture_matrix, ", total samples received:", self.total_samples
					self.samples_since_excess = 0
					
					if self.xform_search is None:
						self.afb.update_lock(0)
						print "Beginning search..."
						self.xform_search = self.xform_lock.copy()
					
					(more, changes) = self.xform_search.next(self.xform_lock, _puncture_matrices[self.puncture_matrix][2])
					if more == False:
						print "Completed XForm cycle"
						if self.fec_found == False:
							self.puncture_matrix = (self.puncture_matrix + 1) % len(_puncture_matrices)
							print "----------------------------"
							print "Trying next puncture matrix:", _puncture_matrices[self.puncture_matrix][0], "[", _puncture_matrices[self.puncture_matrix][1], "]"
							print "----------------------------"
							self.afb.update_matrix(_puncture_matrices[self.puncture_matrix][1])
						else:
							pass	# Keep looping
					self.afb.update_xform(self.xform_search, changes)
					#print "Skipping samples for settling period:", self.settling_period
					self.skip_samples = self.settling_period	# Wait some time for new parameters to take effect
				else:
					self.samples_since_excess += len(samples)
					
					if self.xform_search is not None or self.fec_found == False:
						if self.samples_since_excess > self.pre_lock_duration:
							print "Locking current XForm"
							if self.xform_search is not None:
								self.xform_lock = self.xform_search
								self.xform_search = None
							if self.fec_found == False:
								print "========================================================="
								print "FEC locked:", _puncture_matrices[self.puncture_matrix][0]
								print "========================================================="
								self.fec_found = True
							self.afb.update_lock(1)
				
				#########################
				
				#self.msg_string += msg.to_string()	# body of the msg as a string

				#bytes_needed = (samples) * gr.sizeof_float
				#if (len(self.msg_string) < bytes_needed):
				#	continue

				#records = []
				#start = 0	#self.skip * gr.sizeof_float
				#chan_data = self.msg_string[start:start+bytes_needed]
				#rec = numpy.fromstring (chan_data, numpy.float32)
				#records.append (rec)
				#self.msg_string = ""

				#unused = nsamples - (self.num_plots*self.samples_per_symbol)
				#unused -= (start / gr.sizeof_float)
				#self.skip = self.samples_per_symbol - (unused % self.samples_per_symbol)
				# print "reclen = %d totsamp %d appended %d skip %d start %d unused %d" % (nsamples, self.total_samples, len(rec), self.skip, start/gr.sizeof_float, unused)

				#de = datascope_DataEvent (records, self.samples_per_symbol, self.num_plots)
			
			#wx.PostEvent (self.event_receiver, de)
			#records = []
			#del de

			#self.skip_samples = self.num_plots * self.samples_per_symbol * self.sym_decim   # lower values = more frequent plots, but higher CPU usage
			#self.skip_samples = self.afb.ber_sample_skip -
		print "Auto-FEC thread exiting:", self.getName()

class auto_fec(gr.hier_block2):
	def __init__(self,
		sample_rate,
		ber_threshold=0,	# Above which to do search
		ber_smoothing=0,	# Alpha of BER smoother (0.01)
		ber_duration=0,		# Length before trying next combo
		ber_sample_decimation=1,
		settling_period=0,
		pre_lock_duration=0,
		#ber_sample_skip=0
		**kwargs):
		
		use_throttle = False
		base_duration = 1024
		if sample_rate > 0:
			use_throttle = True
			base_duration *= 4	# Has to be high enough for block-delay
		
		if ber_threshold == 0:
			ber_threshold = 512 * 4
		if ber_smoothing == 0:
			ber_smoothing = 0.01
		if ber_duration == 0:
			ber_duration = base_duration * 2 # 1000ms
		if settling_period == 0:
			settling_period = base_duration * 1 # 500ms
		if pre_lock_duration == 0:
			pre_lock_duration = base_duration * 2 #1000ms
		
		print "Creating Auto-FEC:"
		print "\tsample_rate:\t\t", sample_rate
		print "\tber_threshold:\t\t", ber_threshold
		print "\tber_smoothing:\t\t", ber_smoothing
		print "\tber_duration:\t\t", ber_duration
		print "\tber_sample_decimation:\t", ber_sample_decimation
		print "\tsettling_period:\t", settling_period
		print "\tpre_lock_duration:\t", pre_lock_duration
		print ""
		
		self.sample_rate = sample_rate
		self.ber_threshold = ber_threshold
		#self.ber_smoothing = ber_smoothing
		self.ber_duration = ber_duration
		self.settling_period = settling_period
		self.pre_lock_duration = pre_lock_duration
		#self.ber_sample_skip = ber_sample_skip
		
		self.data_lock = threading.Lock()

		gr.hier_block2.__init__(self, "auto_fec",
			gr.io_signature(1, 1, gr.sizeof_gr_complex),			# Post MPSK-receiver complex input
			gr.io_signature3(3, 3, gr.sizeof_char, gr.sizeof_float, gr.sizeof_float))	# Decoded packed bytes, BER metric, lock
		
		self.input_watcher = auto_fec_input_watcher(self)
		default_xform = self.input_watcher.xform_lock
		
		self.gr_conjugate_cc_0 = blocks.conjugate_cc()
		self.conj_add = None
		try:
			self.gr_conjugate_cc_0.set(True)
		except:
			self.conj_mul_straight = blocks.multiply_const_vcc((0, ))
			self.conj_mul_conj = blocks.multiply_const_vcc((1, ))
			self.conj_add = blocks.add_vcc(1)
			self.connect((self.gr_conjugate_cc_0, 0), (self.conj_mul_conj, 0))    
			self.connect((self.conj_mul_conj, 0), (self.conj_add, 1))    
			self.connect((self.conj_mul_straight, 0), (self.conj_add, 0))    
			self.connect((self, 0), (self.conj_mul_straight, 0))

		self.connect((self, 0), (self.gr_conjugate_cc_0, 0))	# Input
		
		#self.blks2_selector_0 = grc_blks2.selector(
		#	item_size=gr.sizeof_gr_complex*1,
		#	num_inputs=2,
		#	num_outputs=1,
		#	input_index=default_xform.get_conjugation_index(),
		#	output_index=0,
		#)
		#self.connect((self.gr_conjugate_cc_0, 0), (self.blks2_selector_0, 0))
		#self.connect((self, 0), (self.blks2_selector_0, 1))		# Input
		
		# CHANGED
		#self.gr_multiply_const_vxx_3 = blocks.multiply_const_vcc((0.707*(1+1j), ))
		#self.connect((self.blks2_selector_0, 0), (self.gr_multiply_const_vxx_3, 0))
		#self.connect((self.gr_conjugate_cc_0, 0), (self.gr_multiply_const_vxx_3, 0))
		
		self.gr_multiply_const_vxx_2 = blocks.multiply_const_vcc((default_xform.get_rotation(), ))	# phase_mult
		#self.connect((self.gr_multiply_const_vxx_3, 0), (self.gr_multiply_const_vxx_2, 0))
		# CHANGED
		if self.conj_add is not None:
			self.connect((self.conj_add, 0), (self.gr_multiply_const_vxx_2, 0))
		else:
			self.connect((self.gr_conjugate_cc_0, 0), (self.gr_multiply_const_vxx_2, 0))
		
		self.gr_complex_to_float_0_0 = blocks.complex_to_float(1)
		self.connect((self.gr_multiply_const_vxx_2, 0), (self.gr_complex_to_float_0_0, 0))
		
		self.gr_interleave_1 = blocks.interleave(gr.sizeof_float*1)
		self.connect((self.gr_complex_to_float_0_0, 1), (self.gr_interleave_1, 1))
		self.connect((self.gr_complex_to_float_0_0, 0), (self.gr_interleave_1, 0))
		
		self.gr_multiply_const_vxx_0 = blocks.multiply_const_vff((1, ))	# invert
		self.connect((self.gr_interleave_1, 0), (self.gr_multiply_const_vxx_0, 0))
		
		self.baz_delay_2 = baz.delay(gr.sizeof_float*1, default_xform.get_puncture_delay())	# delay_puncture
		self.connect((self.gr_multiply_const_vxx_0, 0), (self.baz_delay_2, 0))
		
		self.depuncture_ff_0 = baz.depuncture_ff((_puncture_matrices[self.input_watcher.puncture_matrix][1]))	# puncture_matrix
		self.connect((self.baz_delay_2, 0), (self.depuncture_ff_0, 0))
		
		self.baz_delay_1 = baz.delay(gr.sizeof_float*1, default_xform.get_viterbi_delay())	# delay_viterbi
		self.connect((self.depuncture_ff_0, 0), (self.baz_delay_1, 0))
		
		self.swap_ff_0 = baz.swap_ff(default_xform.get_viterbi_swap())	# swap_viterbi
		self.connect((self.baz_delay_1, 0), (self.swap_ff_0, 0))
		
		self.gr_decode_ccsds_27_fb_0 = fec.decode_ccsds_27_fb()
		
		if use_throttle:
			print "==> Using throttle at sample rate:", self.sample_rate
			self.gr_throttle_0 = blocks.throttle(gr.sizeof_float, self.sample_rate)
			self.connect((self.swap_ff_0, 0), (self.gr_throttle_0, 0))
			self.connect((self.gr_throttle_0, 0), (self.gr_decode_ccsds_27_fb_0, 0))
		else:
			self.connect((self.swap_ff_0, 0), (self.gr_decode_ccsds_27_fb_0, 0))
		
		self.connect((self.gr_decode_ccsds_27_fb_0, 0), (self, 0))	# Output bytes
		
		self.gr_add_const_vxx_1 = blocks.add_const_vff((-4096, ))
		self.connect((self.gr_decode_ccsds_27_fb_0, 1), (self.gr_add_const_vxx_1, 0))
		
		self.gr_multiply_const_vxx_1 = blocks.multiply_const_vff((-1, ))
		self.connect((self.gr_add_const_vxx_1, 0), (self.gr_multiply_const_vxx_1, 0))
		self.connect((self.gr_multiply_const_vxx_1, 0), (self, 1))	# Output BER
		
		self.gr_single_pole_iir_filter_xx_0 = filter.single_pole_iir_filter_ff(ber_smoothing, 1)
		self.connect((self.gr_multiply_const_vxx_1, 0), (self.gr_single_pole_iir_filter_xx_0, 0))
		
		self.gr_keep_one_in_n_0 = blocks.keep_one_in_n(gr.sizeof_float, ber_sample_decimation)
		self.connect((self.gr_single_pole_iir_filter_xx_0, 0), (self.gr_keep_one_in_n_0, 0))
		
		self.const_source_x_0 = analog.sig_source_f(0, analog.GR_CONST_WAVE, 0, 0, 0)	# Last param is const value
		if use_throttle:
			lock_throttle_rate = self.sample_rate // 16
			print "==> Using lock throttle rate:", lock_throttle_rate
			self.gr_throttle_1 = blocks.throttle(gr.sizeof_float, lock_throttle_rate)
			self.connect((self.const_source_x_0, 0), (self.gr_throttle_1, 0))
			self.connect((self.gr_throttle_1, 0), (self, 2))
		else:
			self.connect((self.const_source_x_0, 0), (self, 2))
		
		self.msg_q = gr.msg_queue(2*256)	# message queue that holds at most 2 messages, increase to speed up process
		self.msg_sink = blocks.message_sink(gr.sizeof_float, self.msg_q, False)	# Block to speed up process
		self.connect((self.gr_keep_one_in_n_0, 0), self.msg_sink)
		
		self.input_watcher.start()
	def update_xform(self, xform, changes=None):
		#with self.data_lock:
			#print "\tBeginning application..."
			if changes is None or auto_fec_xform.CHANGE_ROTATION in changes:
				print "\tApplying rotation:", xform.get_rotation()
				self.gr_multiply_const_vxx_2.set_k((xform.get_rotation(), ))
			if changes is None or auto_fec_xform.CHANGE_CONJUGATION in changes:
				print "\tApplying conjugation:", xform.get_conjugation_index()
				#self.blks2_selector_0.set_input_index(xform.get_conjugation_index())
				if self.conj_add is not None:
					self.conj_mul_straight.set_k((float(xform.get_conjugation_index()),0))
					self.conj_mul_conj.set_k((float(1 - xform.get_conjugation_index()),0))
				else:
					self.gr_conjugate_cc_0.set(xform.get_conjugation_index() == 0)
			if changes is None or auto_fec_xform.CHANGE_INVERSION in changes:
				pass
			if changes is None or auto_fec_xform.CHANGE_PUNCTURE_DELAY in changes:
				print "\tApplying puncture delay:", xform.get_puncture_delay()
				self.baz_delay_2.set_delay(xform.get_puncture_delay())
			if changes is None or auto_fec_xform.CHANGE_VITERBI_DELAY in changes:
				print "\tApplying viterbi delay:", xform.get_viterbi_delay()
				self.baz_delay_1.set_delay(xform.get_viterbi_delay())
			if changes is None or auto_fec_xform.CHANGE_VITERBI_SWAP in changes:
				print "\tApplying viterbi swap:", xform.get_viterbi_swap()
				self.swap_ff_0.set_swap(xform.get_viterbi_swap())
			#print "\tApplication complete."
			print ""
	def update_matrix(self, matrix):
		#with self.data_lock:
			print "\tApplying puncture matrix:", matrix
			self.depuncture_ff_0.set_matrix(matrix)
	def update_lock(self, locked):
		#with self.data_lock:
			print "\tApplying lock value:", locked
			self.const_source_x_0.set_offset(locked)
	def set_sample_rate(self, rate):
		self.sample_rate = rate
		self.input_watcher.set_sample_rate(rate)
	def set_ber_threshold(self, threshold):
		pass
	def set_ber_smoothing(self, smoothing):
		pass
	def set_ber_duration(self, duration):
		pass
	def set_ber_sample_decimation(self, rate):
		self.gr_keep_one_in_n_0.set_n(rate)
	def set_settling_period(self, period):
		pass
	def set_pre_lock_duration(self, duration):
		pass
	def set_puncture_matrix(self, matrix):
		# Not applying, just cause another search
		self.input_watcher.set_puncture_matrix(matrix)
	def set_reset(self, dummy):
		self.input_watcher.set_reset()
