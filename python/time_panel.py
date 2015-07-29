#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  time_panel.py
#  
#  Copyright 2013 Balint Seeber <balint@crawfish>
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

import math, time

import wx

from gnuradio import gr
from gnuradio.wxgui import common

import baz

from time_panel_gen import *

_FUZ = (60 * 60 * 24) * 2	# 2 days tolerance from computer's clock

# http://wiki.wxpython.org/Timer
class time_panel(TimePanel):
	def __init__(self, parent, rate, time_keeper, relative=False, mode=None, **kwds):
		TimePanel.__init__(self, parent)
		self.time_keeper = time_keeper
		self.timer_id = wx.NewId()
		self.relative = relative
		self.set_display_mode(mode)
		self.timer = wx.Timer(self, self.timer_id)
		wx.EVT_TIMER(self, self.timer_id, self.on_timer)
		wx.EVT_CLOSE(self, self.on_close)
		self.set_rate(rate)
	def on_timer(self, event):
		t = self.time_keeper.time(self.relative)
		whole_seconds = int(math.floor(t))
		fractional_seconds = t - whole_seconds
		
		if self.mode == 'absolute' or ((self.mode == 'auto') and (abs(t - time.time()) > _FUZ)):
			seconds = whole_seconds % 60
			minutes = ((whole_seconds - seconds) / 60) % 60
			hours = ((((whole_seconds - seconds) / 60) - minutes) / 60) % 24
			days = ((((((whole_seconds - seconds) / 60) - minutes) / 60) - hours) / 24)
			time_str = "%02i:%02i:%02i:%02i.%03i" % (days, hours, minutes, seconds, int(fractional_seconds*1000))
		else:
			ts = time.localtime(t)
			time_str = time.strftime("%a, %d %b %Y %H:%M:%S", ts)
			time_str += ".%03i" % (int(fractional_seconds*1000))
			#offset = time.timezone / 3600
			#ts.tm_isdst
			tz_str = time.strftime("%Z", ts)
			if len(tz_str) > 0:
				time_str += " " + tz_str
		
		update_count = self.time_keeper.update_count()
		if update_count > 0:
			time_str += " (#%i)" % (update_count)
			
		self.m_staticTime.SetLabel(time_str)
	def on_close(self, event):
		self.timer.Stop()
		self.Destroy()
	def set_rate(self, rate, start=True):
		self.timer.Stop()
		if rate <= 0:
			return
		self.rate = rate
		if start:
			self.timer.Start(int(1000.0/self.rate))
	def set_relative(self, relative):
		self.relative = relative
	def set_display_mode(self, mode=None):
		if mode is None:
			mode = 'auto'
		self.mode = mode

class time_panel_sink(gr.hier_block2, common.wxgui_hb):
	def __init__(self, parent, item_size, sample_rate, rate=1.0, relative=False, mode=None, **kwds):
		gr.hier_block2.__init__(self, "time_panel_sink",
			gr.io_signature(1, 1, item_size),
			gr.io_signature(0, 0, 0)
		)
		self.message_port_register_hier_in('status')
		self.time_keeper = baz.time_keeper(item_size, sample_rate)
		self.msg_connect((self.time_keeper, 'status'), (self, 'status'))
		self.win = time_panel(parent, rate, self.time_keeper, relative, mode)
		self.wxgui_connect(self, self.time_keeper)
	def set_rate(self, rate):
		self.win.set_rate(rate)
	def set_relative(self, relative):
		self.win.set_relative(relative)
	def ignore_next(self, dummy, **kwds):
		self.time_keeper.ignore_next()
	def set_display_mode(self, mode=None):
		self.win.set_display_mode(mode)

def main():
	return 0

if __name__ == '__main__':
	main()
