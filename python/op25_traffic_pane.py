#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  op25_traffic_panel.py
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

import wx
import cPickle as pickle
import gnuradio.gr.gr_threading as _threading

wxDATA_EVENT = wx.NewEventType()

def EVT_DATA_EVENT(win, func):
	win.Connect(-1, -1, wxDATA_EVENT, func)

class DataEvent(wx.PyEvent):
	def __init__(self, data):
		wx.PyEvent.__init__(self)
		self.SetEventType (wxDATA_EVENT)
		self.data = data

	def Clone (self):
		self.__class__ (self.GetId())

class traffic_watcher_thread(_threading.Thread):
	def __init__(self, rcvd_pktq, event_receiver):
		_threading.Thread.__init__(self)
		self.setDaemon(1)
		self.rcvd_pktq = rcvd_pktq
		self.event_receiver = event_receiver
		self.keep_running = True
		self.start()

	def stop(self):
		self.keep_running = False

	def run(self):
		while self.keep_running:
			msg = self.rcvd_pktq.delete_head()
			de = DataEvent (msg)
			wx.PostEvent (self.event_receiver, de)
			del de

# A snapshot of important fields in current traffic
#
class TrafficPane(wx.Panel):

    # Initializer
    #
    def __init__(self, parent, msgq):
        wx.Panel.__init__(self, parent)
        
        self.msgq = msgq
        
        sizer = wx.GridBagSizer(hgap=10, vgap=10)
        self.fields = {}

        label = wx.StaticText(self, -1, "DUID:")
        sizer.Add(label, pos=(1,1))
        field = wx.TextCtrl(self, -1, "", size=(144, -1), style=wx.TE_READONLY)
        sizer.Add(field, pos=(1,2))
        self.fields["duid"] = field;

        label = wx.StaticText(self, -1, "NAC:")
        sizer.Add(label, pos=(2,1))
        field = wx.TextCtrl(self, -1, "", size=(144, -1), style=wx.TE_READONLY)
        sizer.Add(field, pos=(2,2))
        self.fields["nac"] = field;

        label = wx.StaticText(self, -1, "Source:")
        sizer.Add(label, pos=(3,1))
        field = wx.TextCtrl(self, -1, "", size=(144, -1), style=wx.TE_READONLY)
        sizer.Add(field, pos=(3,2))
        self.fields["source"] = field;

        label = wx.StaticText(self, -1, "Destination:")
        sizer.Add(label, pos=(4,1))
        field = wx.TextCtrl(self, -1, "", size=(144, -1), style=wx.TE_READONLY)
        sizer.Add(field, pos=(4,2))
        self.fields["dest"] = field;

        label = wx.StaticText(self, -1, "MFID:")
        sizer.Add(label, pos=(1,4))
        field = wx.TextCtrl(self, -1, "", size=(144, -1), style=wx.TE_READONLY)
        sizer.Add(field, pos=(1,5))
        self.fields["mfid"] = field;

        label = wx.StaticText(self, -1, "ALGID:")
        sizer.Add(label, pos=(2,4))
        field = wx.TextCtrl(self, -1, "", size=(144, -1), style=wx.TE_READONLY)
        sizer.Add(field, pos=(2,5))
        self.fields["algid"] = field;

        label = wx.StaticText(self, -1, "KID:")
        sizer.Add(label, pos=(3,4))
        field = wx.TextCtrl(self, -1, "", size=(144, -1), style=wx.TE_READONLY)
        sizer.Add(field, pos=(3,5))
        self.fields["kid"] = field;

        label = wx.StaticText(self, -1, "MI:")
        sizer.Add(label, pos=(4,4))
        field = wx.TextCtrl(self, -1, "", size=(216, -1), style=wx.TE_READONLY)
        sizer.Add(field, pos=(4,5))
        self.fields["mi"] = field;

        label = wx.StaticText(self, -1, "TGID:")
        sizer.Add(label, pos=(5,4))
        field = wx.TextCtrl(self, -1, "", size=(144, -1), style=wx.TE_READONLY)
        sizer.Add(field, pos=(5,5))
        self.fields["tgid"] = field;

        self.SetSizer(sizer)
        self.Fit()
        
        EVT_DATA_EVENT(self, self.display_data)
        self.watcher = traffic_watcher_thread(self.msgq, self)

    # Clear the field values
    #
    def clear(self):
        for v in self.fields.values():
            v.Clear()
    
    def display_data(self,event):
        message = event.data
        pickled_dict = message.to_string()
        attrs = pickle.loads(pickled_dict)
        self.update(attrs)

    # Update the field values
    #
    def update(self, field_values):
        if field_values['duid'] == 'hdu':
            self.clear()
        for k,v in self.fields.items():
            f = field_values.get(k, None)
            if f:
                v.SetValue(f)

def main():
	return 0

if __name__ == '__main__':
	main()
