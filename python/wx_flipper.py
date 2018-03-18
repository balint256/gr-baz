#!/usr/bin/env python2

import wx

class WX_Flipper():
	def __init__(self, win, target, interval=None, start_now=True, force_idle=False, *args, **kwds):
		assert(win is not None)
		assert(target is not None)
		self.win = win
		# self.timer = None
		self.timer = wx.Timer(win)
		self.interval = interval
		self.target = target
		self.timer_running = False

		if force_idle or interval is None or interval <= 0:
			self.win.Bind(wx.EVT_IDLE, self.OnIdle)
		
		if interval is not None and interval > 0:
			self.win.Bind(wx.EVT_TIMER, self.OnTimer, self.timer)
			if start_now:
				self.start_timer()

		self.win.Bind(wx.EVT_CLOSE, self.OnClose)

	def start_timer(self, interval=None):
		if self.timer_running:
			self.stop_timer()
		if interval is not None:
			self.interval = interval
		self.timer.Start(self.interval)
		self.timer_running = True

	def stop_timer(self):
		if not self.timer_running:
			return
		self.timer.Stop()
		self.timer_running = False

	def OnClose(self, event):
		print "Close"
		pass

	def OnTimer(self, event):
		# print "OnTimer"
		self.target()

	def OnIdle(self, event):
		# print "OnIdle"
		self.target()
