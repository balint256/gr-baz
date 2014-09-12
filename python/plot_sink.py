#
# Copyright 2008,2009,2010 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#

from __future__ import division

##################################################
# Imports
##################################################
from gnuradio import gr, blocks#, blks2
from gnuradio.wxgui import common
from gnuradio.wxgui.pubsub import pubsub
from gnuradio.wxgui.constants import *
import math

import plot_window

##################################################
# Plot sink block (wrapper for old wxgui)
##################################################
class plot_sink_f(gr.hier_block2, common.wxgui_hb):
	"""
	An plotting block with real inputs and a gui window.
	"""
	def __init__(
		self,
		parent,
		ref_scale=2.0,
		y_per_div=10,
		y_divs=8,
		ref_level=50,
		sample_rate=1,
		data_len=1024,
		update_rate=plot_window.DEFAULT_FRAME_RATE,
		average=False,
		avg_alpha=None,
		title='',
		size=plot_window.DEFAULT_WIN_SIZE,
		peak_hold=False,
		use_persistence=False,
		persist_alpha=None,
		**kwargs #do not end with a comma
	):
		#ensure avg alpha
		if avg_alpha is None:
			avg_alpha = 2.0/update_rate
		#ensure analog alpha
		if persist_alpha is None:
			actual_update_rate=float(sample_rate/data_len)/float(max(1,int(float((sample_rate/data_len)/update_rate))))
			#print "requested_specest_rate ",update_rate
			#print "actual_update_rate    ",actual_update_rate
			analog_cutoff_freq=0.5 # Hertz
			#calculate alpha from wanted cutoff freq
			persist_alpha = 1.0 - math.exp(-2.0*math.pi*analog_cutoff_freq/actual_update_rate)
		
		self._average = average
		self._avg_alpha = avg_alpha
		self._sample_rate = sample_rate
		
		#init
		gr.hier_block2.__init__(
			self,
			"plot_sink",
			gr.io_signature(1, 1, gr.sizeof_float*data_len),
			gr.io_signature(0, 0, 0),
		)
		#blocks
		msgq = gr.msg_queue(2)
		sink = blocks.message_sink(gr.sizeof_float*data_len, msgq, True)
		
		#controller
		self.controller = pubsub()
		self.controller.subscribe(AVERAGE_KEY, self.set_average)
		self.controller.publish(AVERAGE_KEY, self.average)
		self.controller.subscribe(AVG_ALPHA_KEY, self.set_avg_alpha)
		self.controller.publish(AVG_ALPHA_KEY, self.avg_alpha)
		self.controller.subscribe(SAMPLE_RATE_KEY, self.set_sample_rate)
		self.controller.publish(SAMPLE_RATE_KEY, self.sample_rate)
		#start input watcher
		common.input_watcher(msgq, self.controller, MSG_KEY)
		#create window
		self.win = plot_window.plot_window(
			parent=parent,
			controller=self.controller,
			size=size,
			title=title,
			data_len=data_len,
			sample_rate_key=SAMPLE_RATE_KEY,
			y_per_div=y_per_div,
			y_divs=y_divs,
			ref_level=ref_level,
			average_key=AVERAGE_KEY,
			avg_alpha_key=AVG_ALPHA_KEY,
			peak_hold=peak_hold,
			msg_key=MSG_KEY,
			use_persistence=use_persistence,
			persist_alpha=persist_alpha,
		)
		common.register_access_methods(self, self.win)
		setattr(self.win, 'set_peak_hold', getattr(self, 'set_peak_hold')) #BACKWARDS
		self.wxgui_connect(self, sink)
	def set_average(self, ave):
		self._average = ave
	def average(self):
		return self._average
	def set_avg_alpha(self, ave):
		self._avg_alpha = ave
	def avg_alpha(self):
		return self._avg_alpha
	def set_sample_rate(self, rate):
		self._sample_rate = rate
	def sample_rate(self):
		return self._sample_rate

# ----------------------------------------------------------------
# Standalone test app
# ----------------------------------------------------------------

import wx
from gnuradio.wxgui import stdgui2

class test_app_block (stdgui2.std_top_block):
	def __init__(self, frame, panel, vbox, argv):
		stdgui2.std_top_block.__init__ (self, frame, panel, vbox, argv)

		data_len = 1024

		# build our flow graph
		input_rate = 2e6

		#Generate some noise
		noise = gr.noise_source_c(gr.GR_GAUSSIAN, 1.0/10)

		# Generate a complex sinusoid
		#source = gr.file_source(gr.sizeof_gr_complex, 'foobar2.dat', repeat=True)

		src1 = gr.sig_source_c (input_rate, gr.GR_SIN_WAVE, -500e3, 1)
		src2 = gr.sig_source_c (input_rate, gr.GR_SIN_WAVE, 500e3, 1)
		src3 = gr.sig_source_c (input_rate, gr.GR_SIN_WAVE, -250e3, 2)

		# We add these throttle blocks so that this demo doesn't
		# suck down all the CPU available.  Normally you wouldn't use these.
		thr1 = gr.throttle(gr.sizeof_gr_complex, input_rate)

		sink1 = plot_sink_f (panel, title="Spectrum Sink", data_len=data_len,
							sample_rate=input_rate,
							ref_level=0, y_per_div=20, y_divs=10)
		vbox.Add (sink1.win, 1, wx.EXPAND)

		combine1=gr.add_cc()
		self.connect(src1,(combine1,0))
		self.connect(src2,(combine1,1))
		self.connect(src3,(combine1,2))
		self.connect(noise, (combine1,3))
		self.connect(combine1,thr1, sink1)

def main ():
	app = stdgui2.stdapp (test_app_block, "Plot Sink Test App")
	app.MainLoop ()

if __name__ == '__main__':
	main ()
