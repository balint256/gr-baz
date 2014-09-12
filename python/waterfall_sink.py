#
# Copyright 2008,2009,2012 Free Software Foundation, Inc.
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

##################################################
# Imports
##################################################
import waterfall_window
from gnuradio.wxgui import common
from gnuradio import gr
from gnuradio import analog
from gnuradio import blocks
from gnuradio.wxgui.pubsub import pubsub
from gnuradio.wxgui.constants import *

##################################################
# Waterfall sink block (wrapper for old wxgui)
##################################################

class waterfall_sink_f(gr.hier_block2, common.wxgui_hb):
	"""
	An fft block with real input and a gui window.
	"""

	def __init__(
		self,
		parent,
		x_offset=0,
		ref_level=50,
		#sample_rate=1,
		data_len=512*2,
		#fft_rate=waterfall_window.DEFAULT_FRAME_RATE,
		#average=False,
		#avg_alpha=None,
		title='',
		size=waterfall_window.DEFAULT_WIN_SIZE,
		ref_scale=2.0,
		dynamic_range=80,
		num_lines=256,
		win=None,
		always_run=False,
		**kwargs #do not end with a comma
	):
		#ensure avg alpha
		#if avg_alpha is None: avg_alpha = 2.0/fft_rate
		#init
		gr.hier_block2.__init__(
			self,
			"waterfall_sink",
			gr.io_signature(1, 1, gr.sizeof_float * data_len),
			gr.io_signature(0, 0, 0),
		)
		#blocks
		msgq = gr.msg_queue(2)
		sink = blocks.message_sink(gr.sizeof_float*data_len, msgq, True)
		#controller
		self.controller = pubsub()
		#start input watcher
		common.input_watcher(msgq, self.controller, MSG_KEY)
		#create window
		self.win = waterfall_window.waterfall_window(
			parent=parent,
			controller=self.controller,
			size=size,
			title=title,
			data_len=data_len,
			num_lines=num_lines,
			x_offset=x_offset,
			#decimation_key=DECIMATION_KEY,
			#sample_rate_key=SAMPLE_RATE_KEY,
			#frame_rate_key=FRAME_RATE_KEY,
			dynamic_range=dynamic_range,
			ref_level=ref_level,
			#average_key=AVERAGE_KEY,
			#avg_alpha_key=AVG_ALPHA_KEY,
			msg_key=MSG_KEY,
		)
		common.register_access_methods(self, self.win)
		setattr(self.win, 'set_baseband_freq', getattr(self, 'set_baseband_freq')) #BACKWARDS	# FIXME
		#connect
		if always_run:
			connect_fn = self.connect
		else:
			connect_fn = self.wxgui_connect
		
		connect_fn(self, sink)

	def set_callback(self,callb):
		self.win.set_callback(callb)
