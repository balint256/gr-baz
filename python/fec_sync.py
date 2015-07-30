#!/usr/bin/env python
# 
# Copyright 2014 Balint Seeber <balint256@gmail.com>
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

import time

from gnuradio import gr
import pmt

_phase_multiplication = [
		('0', 1),
		('90', 1j)
]

class fec_sync_xform():
	CHANGE_PUNCTURE_DELAY = 1
	CHANGE_ROTATION = 2
	CHANGE_CONJUGATION = 3
	#_clonable = [
	#		'puncture_delay'
	#		'rotation',
	#		'conjugate',
	#	]
	def __init__(self):
		self.reset()
	def reset(self):
		self.conjugate = True
		self.rotation = 0
		self.puncture_delay = 0
	def copy(self):
		clone = fec_sync_xform()
		#for k in fec_sync_xform._clonable:
		#	clone[k] = self[k]
		clone.puncture_delay = self.puncture_delay
		clone.rotation = self.rotation
		clone.conjugate = self.conjugate
		return clone
	def get_conjugation(self):
		return self.conjugate
	def get_rotation(self):
		return _phase_multiplication[self.rotation][1]
	def get_puncture_delay(self):
		return self.puncture_delay
	def next(self, ref, depunc_length):	#, psk_order=4
		changes = []
		
		changes += [fec_sync_xform.CHANGE_PUNCTURE_DELAY]
		self.puncture_delay = (self.puncture_delay + 1) % depunc_length
		if self.puncture_delay != ref.puncture_delay:
			return (True, changes)
		
		changes += [fec_sync_xform.CHANGE_ROTATION]
		self.rotation = (self.rotation + 1) % len(_phase_multiplication)	# FIXME: Handle arbitrary PSK order
		if self.rotation != ref.rotation:
			return (True, changes)
		
		changes += [fec_sync_xform.CHANGE_CONJUGATION]
		self.conjugate = not self.conjugate
		if self.conjugate != ref.conjugate:
			return (True, changes)
		
		return (False, changes)

class fec_sync(gr.basic_block):
    """
    docstring for block fec_sync
    """
    def __init__(self, conj, rot, depunc_delay, depunc_length, trial_duration, lock_timeout, verbose=False):	#, expected_pdu_rate
        gr.basic_block.__init__(self,
            name="fec_sync",
            in_sig=None,
            out_sig=None)
        
        self.conj = conj
        self.rot = rot
        self.depunc_delay = depunc_delay
        self.depunc_length = depunc_length
        #self.expected_pdu_rate = expected_pdu_rate
        #self.expected_pdu_period = 1.0 / expected_pdu_rate
        self.trial_duration = trial_duration
        self.lock_timeout = lock_timeout
        self.verbose = verbose

        print "Depuncturer length:", depunc_length
        print "Trial duration:", trial_duration
        print "Lock timeout:", lock_timeout

        self.update_interval = 1.0
        self.last_update_time = None
        self.last_pdu_count = 0

        self.last_time = None
        self.locked = False
        self.xform_lock = fec_sync_xform()
        self.xform_search = fec_sync_xform()
        self.last_pdu_time = None
        self.last_xform_time = None
        self.search_iterations = 0

        self.message_port_register_in(pmt.intern('clock'))
        self.set_msg_handler(pmt.intern('clock'), self.handle_clock)
        self.message_port_register_in(pmt.intern('pdu'))
        self.set_msg_handler(pmt.intern('pdu'), self.handle_pdu)
        self.message_port_register_in(pmt.intern('status'))
        self.set_msg_handler(pmt.intern('status'), self.handle_status)

        self.set_unlocked()

    def general_work(self, input_items, output_items):
    	print "general_work"
    	return 0

    def handle_clock(self, msg):
    	# pmt.PMT_T
    	self.run()

    def handle_status(self, msg):
    	#print "Received status message:", msg
    	#pmt.intern("overflow");

    	# FIXME: May need to toggle to use longer unlock timeout in case it can be re-acquired with current xform?
    	# Let it timeout
    	#self.set_unlocked()
    	return

    def set_unlocked(self):
    	self.locked = False
    	print "[FEC] Resetting xform"
    	self.xform_lock.reset()
    	self.xform_search.reset()
    	self.search_iterations = 0
    	self.update_xform(self.xform_search)

    def set_locked(self):
    	self.locked = True
    	print "[FEC] Locked!"
    	self.xform_lock = self.xform_search.copy()

    def handle_pdu(self, msg):
        #try:
        #    meta = pmt.car(msg)
        #    data =  pmt.cdr(msg)
        #except:
        #    print "[FEC] Message is not a PDU"
        #    return
        
        #if pmt.is_u8vector(data):
        #    data = pmt.u8vector_elements(data)
        #else:
        #    print "[FEC] Data is not a u8vector"
        #    return
        
        #meta_dict = pmt.to_python(meta)
        #if not (type(meta_dict) is dict):
        #    meta_dict = {}

        self.last_pdu_count += 1

        self.last_pdu_time = time.time()

        if not self.locked:
        	self.set_locked()

    def update_xform(self, xform, changes=None, time_now=None):
		self.search_iterations += 1

		#print "[FEC] Applying xform..."

		if changes is None or fec_sync_xform.CHANGE_ROTATION in changes:
			if self.verbose: print "[FEC] \t[%03d] Applying rotation:      " % (self.search_iterations), xform.get_rotation()
			self.rot.set_k((xform.get_rotation(), ))
		if changes is None or fec_sync_xform.CHANGE_CONJUGATION in changes:
			if self.verbose: print "[FEC] \t[%03d] Applying conjugation:   " % (self.search_iterations), xform.get_conjugation()
			self.conj.set(xform.get_conjugation())
		if changes is None or fec_sync_xform.CHANGE_PUNCTURE_DELAY in changes:
			if self.verbose: print "[FEC] \t[%03d] Applying puncture delay:" % (self.search_iterations), xform.get_puncture_delay()
			self.depunc_delay.set_delay(xform.get_puncture_delay())

		#print ""

		if time_now is None:
			time_now = time.time()
		self.last_xform_time = time_now

    def run(self):
    	time_diff = 0
    	time_now = time.time()
    	if self.last_time is not None:
    		time_diff = time_now - self.last_time
    		#print "[FEC] Running... (time diff: %.4f)" % (time_diff)
    	self.last_time = time_now

    	if self.last_update_time is not None:
    		diff = time_now - self.last_update_time
    		if diff > self.update_interval:
    			#if self.last_pdu_count > 0:
    			#if True:
    			#	print "[FEC] Received %d PDUs in %.4f seconds" % (self.last_pdu_count, diff)
    			self.last_pdu_count = 0
    	else:
    		self.last_update_time = time_now

    	if self.locked:
    		diff = time_now - self.last_pdu_time
    		if diff > self.lock_timeout:	
    			print "[FEC] Lock timed out after %.4f seconds!" % (diff)
    			self.set_unlocked()
    	#else
    	if not self.locked:
    		diff = time_now - self.last_xform_time
    		if diff > self.trial_duration:
    			#print "Still unlocked after %.4f seconds" % (diff)

    			(more, changes) = self.xform_search.next(self.xform_lock, self.depunc_length)
    			if not more:
    				if self.verbose: print "[FEC] Cycling search..."

    			self.update_xform(self.xform_search, changes, time_now)
    	#
