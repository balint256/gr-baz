#!/usr/bin/env python

import wx
import wx.glcanvas
from OpenGL import GL
from gnuradio.wxgui.plotter.plotter_base import plotter_base
from gnuradio.wxgui.plotter import gltext
import math

COMPASS_LINE_COLOR_SPEC = (.6, .6, .6)
TXT_FONT_SIZE = 13
TXT_SCALE_FACTOR = 1000
TXT_RAD = 0.465
CIRCLE_RAD = 0.4
BIG_TICK_LEN = 0.05
MED_TICK_LEN = 0.035
SMALL_TICK_LEN = 0.01
TICK_LINE_WIDTH = 1.5
PROFILE_LINE_WIDTH = 1.5

def polar2rect(*coors):
	return [(r*math.cos(math.radians(a)), r*math.sin(math.radians(a))) for r, a in coors]

class compass_plotter(plotter_base):

	def __init__(self, parent):
		plotter_base.__init__(self, parent)
		#setup profile cache
		self._profiles = dict()
		self._profile_cache = self.new_gl_cache(self._draw_profile, 50)
		#setup compass cache
		self._compass_cache = self.new_gl_cache(self._draw_compass, 25)
		self._text_cache = self.new_gl_cache(self._draw_text, 65)
		self._text = None
		self._text_visible = False
		self._gl_text = None
		#init compass plotter
		self.register_init(self._init_compass_plotter)

	def _init_compass_plotter(self):
		"""
		Run gl initialization tasks.
		"""
		GL.glEnableClientState(GL.GL_VERTEX_ARRAY)

	def _setup_antialiasing(self):
		#enable antialiasing
		GL.glEnable(GL.GL_BLEND)
		GL.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA)
		for type, hint in (
			(GL.GL_LINE_SMOOTH, GL.GL_LINE_SMOOTH_HINT),
			(GL.GL_POINT_SMOOTH, GL.GL_POINT_SMOOTH_HINT),
			(GL.GL_POLYGON_SMOOTH, GL.GL_POLYGON_SMOOTH_HINT),
		):
			GL.glEnable(type)
			GL.glHint(hint, GL.GL_NICEST)

	def _draw_compass(self):
		"""
		Draw the compass rose tick marks and tick labels.
		"""
		self._setup_antialiasing()
		GL.glLineWidth(TICK_LINE_WIDTH)
		#calculate points for the compass
		points = list()
		for degree in range(0, 360):
			if degree%10 == 0: tick_len = BIG_TICK_LEN
			elif degree%5 == 0: tick_len = MED_TICK_LEN
			else: tick_len = SMALL_TICK_LEN
			points.append((CIRCLE_RAD, degree))
			points.append((CIRCLE_RAD+tick_len, degree))
		points = polar2rect(*points)
		#scale with matrix transform
		GL.glPushMatrix()
		GL.glScalef(self.width, self.height, 1)
		GL.glTranslatef(0.5, 0.5, 0)
		GL.glRotatef(-90, 0, 0, 1)
		#set color and draw
		GL.glColor3f(*COMPASS_LINE_COLOR_SPEC)
		GL.glVertexPointerf(points)
		GL.glDrawArrays(GL.GL_LINES, 0, len(points))
		#draw the labels
		GL.glScalef(1.0/TXT_SCALE_FACTOR, 1.0/TXT_SCALE_FACTOR, 1)
		for degree in range(0, 360, 10):
			position = wx.Point(*(polar2rect((TXT_SCALE_FACTOR*TXT_RAD, degree))[0]))
			txt = gltext.Text('%d'%degree, font_size=TXT_FONT_SIZE, centered=True)
			txt.draw_text(position=position, rotation=-degree-90)
		GL.glPopMatrix()
	
	def _draw_text(self):
		#if self._text is None or len(self._text) == 0:
			#return
		#if type(self._text_visible) == bool and self._text_visible == False:
			#return
		#if type(self._text_visible) == int and self._text_visible < 0:
			#return
		
		#text = self._text
		#idx = self._text_visible
		#if type(idx) == bool:
			#idx = 0
		#if type(text) == list:
			#text = text[idx]
		
		self._setup_antialiasing()
		GL.glPushMatrix()
		GL.glScalef(self.width, self.height, 1)
		GL.glTranslatef(0.5, 0.65, 0)
		#GL.glRotatef(-90, 0, 0, 1)
		GL.glScalef(1.0/TXT_SCALE_FACTOR, 1.0/TXT_SCALE_FACTOR, 1)
		#GL.glScalef(1.0, 1.0, 1)
		position = wx.Point(*(.0,.0))
		if self._gl_text is not None:
			txt = gltext.Text(self._gl_text, font_size=96, centered=True, foreground=wx.RED)
			txt.draw_text(position=position, rotation=0)
		GL.glPopMatrix()
		#print "Drawn"

	def _draw_profile(self):
		"""
		Draw the profiles into the compass rose as polygons.
		"""
		self._setup_antialiasing()
		GL.glLineWidth(PROFILE_LINE_WIDTH)
		#scale with matrix transform
		GL.glPushMatrix()
		GL.glScalef(self.width, self.height, 1)
		GL.glTranslatef(0.5, 0.5, 0)
		GL.glScalef(CIRCLE_RAD, CIRCLE_RAD, 1)
		GL.glRotatef(-90, 0, 0, 1)
		#draw the profile
		for key in sorted(self._profiles.keys()):
			color_spec, fill, profile = self._profiles[key]
			if not profile: continue
			points = polar2rect(*profile)
			GL.glColor3f(*color_spec)
			GL.glVertexPointerf(points)
			GL.glDrawArrays(fill and GL.GL_POLYGON or GL.GL_LINE_LOOP, 0, len(points))
		GL.glPopMatrix()

	def set_profile(self, key='', color_spec=(0, 0, 0), fill=True, profile=[]):
		"""
		Set a profile onto the compass rose.
		A polar coordinate tuple is of the form (radius, angle).
		Where radius is between -1 and 1 and angle is in degrees.
		@param key unique identifier for profile
		@param color_spec a 3-tuple gl color spec
		@param fill true to fill in the polygon or false for outline
		@param profile a list of polar coordinate tuples
		"""
		self.lock()
		self._profiles[key] = color_spec, fill, profile
		self._profile_cache.changed(True)
		self.unlock()
	
	def set_text(self, text, visible=None):
		#print "set_text", text, visible
		if self._text == text:
			return
		self._text = text
		if visible is not None:
			self._text_visible = visible
		self._update_text()
	
	def set_text_visible(self, visible, force=False):
		#if type(visible) == float or type(visible) == int:
		#	visible = visible != 0
		if force == False and self._text_visible == visible:
			return
		#print "set_text_visible", self._text, visible
		self._text_visible = visible
		self._update_text()
	
	def _update_text(self):
		_text = None
		
		if self._text is None or len(self._text) == 0:
			return
		if type(self._text_visible) == bool and self._text_visible == False:
			return
		if type(self._text_visible) == int and self._text_visible < 0:
			return
		
		_text = self._text
		idx = self._text_visible
		if type(idx) == bool:
			idx = 0
		if type(self._text) == list:
			_text = self._text[idx]
		
		self.lock()
		if _text is None:
			self._gl_text = None
		else:
			#print _text
			self._gl_text = _text
		self._text_cache.changed(True)
		self.unlock()

if __name__ == '__main__':
	app = wx.PySimpleApp()
	frame = wx.Frame(None, -1, 'Demo', wx.DefaultPosition)
	vbox = wx.BoxSizer(wx.VERTICAL)

	plotter = compass_plotter(frame)
	import random
	plotter.set_profile(key='1', color_spec=(1, 1, 0), fill=True, profile=[(random.random(), degree) for degree in range(0, 360, 3)])
	plotter.set_profile(key='2', color_spec=(0, 0, 1), fill=False, profile=[(random.random(), degree) for degree in range(0, 360, 3)])
	plotter.set_text("Hello World", True)
	vbox.Add(plotter, 1, wx.EXPAND)

	frame.SetSizerAndFit(vbox)
	frame.SetSize(wx.Size(600, 600))
	frame.Show()
	app.MainLoop()
