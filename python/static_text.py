#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  static_text.py
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

import threading
import wx
#from gnuradio.wxgui import forms
from gnuradio.wxgui.forms import forms
from gnuradio.wxgui.forms.forms import *
from gnuradio.wxgui.forms import converters
from gnuradio.wxgui.pubsub import pubsub

def make_bold(widget):
	font = widget.GetFont()
	font.SetWeight(wx.FONTWEIGHT_BOLD)
	widget.SetFont(font)

#class panel_base(forms._form_base):
	#def __init__(self, converter, **kwargs):
	#	forms._form_base.__init__(self, converter=converter, **kwargs)
class panel_base2(pubsub, wx.Panel):
	def __init__(self, parent=None, sizer=None, proportion=0, flag=wx.EXPAND, ps=None, key='', value=None, callback=None, converter=converters.identity_converter(), size=None, *args, **kwds):
		pubsub.__init__(self)
		#wx.BoxSizer.__init__(self, wx.VERTICAL)
		if size is not None and size != (-1, -1):
			print "Size:", size
		#	kwds['size'] = size
		#wx.Panel.__init__(self, parent, *args, **kwds)
		wx.Panel.__init__ ( self, parent, id = wx.ID_ANY, pos = wx.DefaultPosition, size = wx.Size( 500,300 ), style = wx.TAB_TRAVERSAL )
		if size is not None and size != (-1, -1):
			self.SetMinSize(size)
			#self.SetSizeHints(*size)
		self._parent = parent
		self._key = key
		self._converter = converter
		self._callback = callback
		self._widgets = list()
		#add to the sizer if provided
		if sizer: sizer.Add(self, proportion, flag)
		#proxy the pubsub and key into this form
		if ps is not None:
			assert key
			self.proxy(EXT_KEY, ps, key)
		#no pubsub passed, must set initial value
		else: self.set_value(value)
		
		self._sizer = wx.BoxSizer(wx.VERTICAL)
		if size is not None and size != (-1, -1):
			self._sizer.SetMinSize(size)
			self._sizer.SetSizeHints(self)

		self.SetSizer(self._sizer)
		
		self.SetBackgroundColour(wx.BLACK)
	
	def __str__(self):
		return "Form: %s -> %s"%(self.__class__, self._key)

	def _add_widget(self, widget, label='', flag=0, label_prop=0, widget_prop=1, sizer=None):
		"""
		Add the main widget to this object sizer.
		If label is passed, add a label as well.
		Register the widget and the label in the widgets list (for enable/disable).
		Bind the update handler to the widget for data events.
		This ensures that the gui thread handles updating widgets.
		Setup the pusub triggers for external and internal.
		@param widget the main widget
		@param label the optional label
		@param flag additional flags for widget
		@param label_prop the proportion for the label
		@param widget_prop the proportion for the widget
		"""
		#setup data event
		widget.Bind(EVT_DATA, lambda x: self._update(x.data))
		update = lambda x: wx.PostEvent(widget, DataEvent(x))
		
		#register widget
		self._widgets.append(widget)
		
		widget_flags = wx.ALIGN_CENTER | flag # wx.ALIGN_CENTER_VERTICAL
		#create optional label
		if label:
			print "Label:", label
			label_text = wx.StaticText(self._parent, label='%s: '%label)
			self._widgets.append(label_text)
			self.Add(label_text, label_prop, wx.ALIGN_CENTER_VERTICAL | wx.ALIGN_LEFT)
			widget_flags = widget_flags | wx.ALIGN_RIGHT
		
		to_add = sizer or widget
		self._sizer.Add(to_add, widget_prop, widget_flags)
		#self.Add(widget, widget_prop, widget_flags)
		
		#self._parent.Layout()
		#self.Layout()
		#self._parent.Layout()
		self.Layout()
		
		#initialize without triggering pubsubs
		self._translate_external_to_internal(self[EXT_KEY])
		update(self[INT_KEY])
		
		#subscribe all the functions
		self.subscribe(INT_KEY, update)
		self.subscribe(INT_KEY, self._translate_internal_to_external)
		self.subscribe(EXT_KEY, self._translate_external_to_internal)

	def _translate_external_to_internal(self, external):
		try:
			internal = self._converter.external_to_internal(external)
			#prevent infinite loop between internal and external pubsub keys by only setting if changed
			if self[INT_KEY] != internal: self[INT_KEY] = internal
		except Exception, e:
			self._err_msg(external, e)
			self[INT_KEY] = self[INT_KEY] #reset to last good setting

	def _translate_internal_to_external(self, internal):
		try:
			external = self._converter.internal_to_external(internal)
			#prevent infinite loop between internal and external pubsub keys by only setting if changed
			if self[EXT_KEY] != external: self[EXT_KEY] = external
		except Exception, e:
			self._err_msg(internal, e)
			self[EXT_KEY] = self[EXT_KEY] #reset to last good setting
		if self._callback: self._callback(self[EXT_KEY])

	def _err_msg(self, value, e):
		print >> sys.stderr, self, 'Error translating value: "%s"\n\t%s\n\t%s'%(value, e, self._converter.help())

	#override in subclasses to handle the wxgui object
	def _update(self, value): raise NotImplementedError
	def _handle(self, event): raise NotImplementedError

	#provide a set/get interface for this form
	def get_value(self): return self[EXT_KEY]
	def set_value(self, value): self[EXT_KEY] = value

	def Disable(self, disable=True): self.Enable(not disable)
	def Enable(self, enable=True):
		if enable:
			for widget in self._widgets: widget.Enable()
		else:
			for widget in self._widgets: widget.Disable()


class panel_base(forms._form_base):
	def __init__(self, converter, orientation = wx.VERTICAL, size = (-1,-1), **kwargs):
		if size is not None and size != (-1, -1):
			print "Size:", size
		#	kwds['size'] = size
		
		forms._form_base.__init__(self, converter=converter, **kwargs)
		
		self.SetOrientation(orientation)
		self.SetMinSize(size)
		#wx.Panel.__init__(self, parent, *args, **kwds)
		self._creator_thread = threading.current_thread()
	
	def _add_widget(self, widget, label='', flag=0, label_prop=0, widget_prop=1):
		"""
		Add the main widget to this object sizer.
		If label is passed, add a label as well.
		Register the widget and the label in the widgets list (for enable/disable).
		Bind the update handler to the widget for data events.
		This ensures that the gui thread handles updating widgets.
		Setup the pusub triggers for external and internal.
		@param widget the main widget
		@param label the optional label
		@param flag additional flags for widget
		@param label_prop the proportion for the label
		@param widget_prop the proportion for the widget
		"""
		#setup data event
		widget.Bind(EVT_DATA, lambda x: self._update(x.data))
		update = lambda x: wx.PostEvent(widget, DataEvent(x))
		
		#register widget
		self._widgets.append(widget)
		
		widget_flags = wx.ALIGN_CENTER | flag # wx.ALIGN_CENTER_VERTICAL
		#create optional label
		if label:
			label_text = wx.StaticText(self._parent, label='%s: '%label)
			self._widgets.append(label_text)
			self.Add(label_text, label_prop, wx.ALIGN_CENTER_VERTICAL | wx.ALIGN_LEFT)
			widget_flags = widget_flags# | wx.ALIGN_RIGHT
		
		self.Add(widget, widget_prop, widget_flags)
		
		#initialize without triggering pubsubs
		self._translate_external_to_internal(self[EXT_KEY])
		update(self[INT_KEY])
		
		#subscribe all the functions
		self.subscribe(INT_KEY, update)
		self.subscribe(INT_KEY, self._translate_internal_to_external)
		self.subscribe(EXT_KEY, self._translate_external_to_internal)

wxUPDATE_EVENT = wx.NewEventType()

def EVT_UPDATE_EVENT(win, func):
	win.Connect(-1, -1, wxUPDATE_EVENT, func)

class UpdateEvent(wx.PyEvent):
	def __init__(self, data):
		wx.PyEvent.__init__(self)
		self.SetEventType(wxUPDATE_EVENT)
		self.data = data
	def Clone (self):
		self.__class__(self.GetId())

class static_text(panel_base):
	"""
	A text box form.
	@param parent the parent widget
	@param sizer add this widget to sizer if provided (optional)
	@param proportion the proportion when added to the sizer (default=0)
	@param flag the flag argument when added to the sizer (default=wx.EXPAND)
	@param ps the pubsub object (optional)
	@param key the pubsub key (optional)
	@param value the default value (optional)
	@param label title label for this widget (optional)
	@param width the width of the form in px
	@param bold true to bold-ify the text (default=False)
	@param units a suffix to add after the text
	@param converter forms.str_converter(), int_converter(), float_converter()...
	"""
	def __init__(self, label='', size=(-1,-1), bold=False, font_size=16, units='', converter=converters.str_converter(), **kwargs):
		self._units = units
		panel_base.__init__(self, converter=converter, size=size, **kwargs)
		
		self._static_text = wx.StaticText(self._parent)	#, size=size #, style=wx.ALIGN_CENTRE | wx.ST_NO_AUTORESIZE
		#self._static_text.Wrap(-1)
		#self._static_text.SetMinSize(size)
		font = wx.Font(font_size, wx.DEFAULT, wx.NORMAL, wx.NORMAL, 0, "")
		#font = wx.Font( 36, 70, 90, 90, False, wx.EmptyString )
		self._static_text.SetFont(font)
		#self._static_text.SetBackgroundColour(wx.GREEN)
		
		#self._static_text = wx.StaticText( self, wx.ID_ANY, u"MyLabel", wx.DefaultPosition, wx.Size( -1,-1 ), 0 )
		#self._static_text.Wrap( -1 )
		#self._static_text.SetFont(  )
		#self._static_text.SetForegroundColour( wx.SystemSettings.GetColour( wx.SYS_COLOUR_INFOBK ) )
		#self._static_text.SetBackgroundColour( wx.SystemSettings.GetColour( wx.SYS_COLOUR_INACTIVECAPTIONTEXT ) )
		
		if bold:
			make_bold(self._static_text)
		
		#self.bSizerV = wx.BoxSizer( wx.VERTICAL )
		#self.bSizerV.Add( self._static_text, 0, wx.ALIGN_CENTER, 5 ) # 5 is border
		#self.SetOrientation(wx.VERTICAL)
		
		self._add_widget(self._static_text, label)
		#self._add_widget(self._static_text, label, sizer=self.bSizerV)
		
		#self.Layout()
		
		EVT_UPDATE_EVENT(self._parent, self.posted_update)
	
	def posted_update(self, event):
		data = event.data
		#print data
		data['fn'](*data['args'])
	
	def _update(self, label):
		if threading.current_thread() != self._creator_thread:
			wx.PostEvent(self._parent, UpdateEvent({'fn': self._update, 'args': [label]}))
			return
		if self._units:
			label += ' ' + self._units
		self._static_text.SetLabel(label)
		#self._parent.Layout()
		self.Layout()
	
	def set_colour(self, colour):
		if threading.current_thread() != self._creator_thread:
			wx.PostEvent(self._parent, UpdateEvent({'fn': self.set_colour, 'args': [colour]}))
			return
		if isinstance(colour, str):
			colour = eval(colour)
		self._static_text.SetForegroundColour(colour)

def main():
	return 0

if __name__ == '__main__':
	main()
