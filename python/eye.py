#!/usr/bin/env python

"""
Draws an eye diagram of an incoming float sample stream.
Certain parameters are adjustable at runtime (also exposed in GRC block).
This code is based on the Data Scope in OP25 (http://op25.osmocom.org/), which was created by Max Parke (KA1RBI).
This file is part of gr-baz. More info: http://wiki.spench.net/wiki/gr-baz
By Balint Seeber (http://spench.net/contact)
"""

from __future__ import with_statement

import threading, math
import wx
import numpy

from gnuradio import gr
import gnuradio.wxgui.plot as plot

# sample_rate/v_scale/t_scale is unused

# add datascope
#self.data_scope = datascope_sink_f(self.notebook, samples_per_symbol = 10, num_plots = 100)
#self.data_plotter = self.data_scope.win.graph
#wx.EVT_RADIOBOX(self.data_scope.win.radio_box, 11103, self.filter_select)
#wx.EVT_RADIOBOX(self.data_scope.win.radio_box_speed, 11104, self.speed_select)
#self.data_scope.win.radio_box_speed.SetSelection(self.current_speed)
#self.notebook.AddPage(self.data_scope.win, "Datascope")

default_scopesink_size = (640, 320)
default_v_scale = None  #1000
default_frame_decim = gr.prefs().get_long('wxgui', 'frame_decim', 1)

#####################################################################

#speeds = [300, 600, 900, 1200, 1440, 1800, 1920, 2400, 2880, 3200, 3600, 3840, 4000, 4800, 6400, 7200, 8000, 9600, 14400, 19200]

#class window_with_ctlbox(wx.Panel):
#    def __init__(self, parent, id = -1):
#        wx.Panel.__init__(self, parent, id)
#    def make_control_box (self):
#        global speeds
#        ctrlbox = wx.BoxSizer (wx.HORIZONTAL)
#
#        ctrlbox.Add ((5,0) ,0)
#
#        run_stop = wx.Button (self, 11102, "Run/Stop")
#        run_stop.SetToolTipString ("Toggle Run/Stop mode")
#        wx.EVT_BUTTON (self, 11102, self.run_stop)
#        ctrlbox.Add (run_stop, 0, wx.EXPAND)
#
#        self.radio_box = wx.RadioBox(self, 11103, "Viewpoint", style=wx.RA_SPECIFY_ROWS,
#                        choices = ["Raw", "Filtered"] )
#        self.radio_box.SetToolTipString("Viewpoint Before Or After Symbol Filter")
#        self.radio_box.SetSelection(1)
#        ctrlbox.Add (self.radio_box, 0, wx.EXPAND)
#
#        ctrlbox.Add ((5, 0) ,0)            # stretchy space
#
#        speed_str = []
#        for speed in speeds:
#            speed_str.append("%d" % speed)
#            
#            self.radio_box_speed = wx.RadioBox(self, 11104, "Symbol Rate", style=wx.RA_SPECIFY_ROWS, majorDimension=2, choices = speed_str)
#            self.radio_box_speed.SetToolTipString("Symbol Rate")
#            ctrlbox.Add (self.radio_box_speed, 0, wx.EXPAND)
#            ctrlbox.Add ((10, 0) ,1)            # stretchy space
#        
#        return ctrlbox

#####################################################################

class eye_sink_f(gr.hier_block2):
    def __init__(self,
            parent,
            title='Eye Diagram',
            sample_rate=1,
            size=default_scopesink_size,
            frame_decim=default_frame_decim,
            samples_per_symbol=10,
            num_plots=100,
            sym_decim=20,
            v_scale=default_v_scale,
            t_scale=None,
            num_inputs=1,
            **kwargs):
        
        if t_scale == 0:
            t_scale = None
        if v_scale == 0:
            v_scale = default_v_scale

        gr.hier_block2.__init__(self, "datascope_sink_f",
                                gr.io_signature(num_inputs, num_inputs, gr.sizeof_float),
                                gr.io_signature(0,0,0))

        msgq = gr.msg_queue(2)  # message queue that holds at most 2 messages
        self.st = gr.message_sink(gr.sizeof_float, msgq, dont_block=1)
        self.connect((self, 0), self.st)

        self.win = datascope_window(
            datascope_win_info(
                msgq,
                sample_rate,
                frame_decim,
                v_scale,
                t_scale,
                #None,  # scopesink (not used)
                title=title),
            parent,
            samples_per_symbol=samples_per_symbol,
            num_plots=num_plots,
            sym_decim=sym_decim,
            size=size)
    def set_sample_rate(self, sample_rate):
        #self.guts.set_sample_rate(sample_rate)
        self.win.info.set_sample_rate(sample_rate)
    def set_samples_per_symbol(self, samples_per_symbol):
        self.win.set_samples_per_symbol(samples_per_symbol)
    def set_num_plots(self, num_plots):
        self.win.set_num_plots(num_plots)
    def set_sym_decim(self, sym_decim):
        self.win.set_sym_decim(sym_decim)

wxDATA_EVENT = wx.NewEventType()

def EVT_DATA_EVENT(win, func):
    win.Connect(-1, -1, wxDATA_EVENT, func)

class datascope_DataEvent(wx.PyEvent):
    def __init__(self, data, samples_per_symbol, num_plots):
        wx.PyEvent.__init__(self)
        self.SetEventType (wxDATA_EVENT)
        self.data = data
        self.samples_per_symbol = samples_per_symbol
        self.num_plots = num_plots
    def Clone (self): 
        self.__class__ (self.GetId())

class datascope_win_info (object):
    __slots__ = ['msgq', 'sample_rate', 'frame_decim', 'v_scale', 
                 'scopesink', 'title',
                 'time_scale_cursor', 'v_scale_cursor', 'marker', 'xy',
                 'autorange', 'running']
    def __init__ (self, msgq, sample_rate, frame_decim, v_scale, t_scale,
                  scopesink=None, title = "Oscilloscope", xy=False):
        self.msgq = msgq
        self.sample_rate = sample_rate
        self.frame_decim = frame_decim
        self.scopesink = scopesink
        self.title = title
        self.v_scale = v_scale
        self.marker = 'line'
        self.xy = xy
        self.autorange = not v_scale
        self.running = True
    def set_sample_rate(self, sample_rate):
        self.sample_rate = sample_rate
    def get_sample_rate (self):
        return self.sample_rate
    #def get_decimation_rate (self):
    #    return 1.0
    def set_marker (self, s):
        self.marker = s
    def get_marker (self):
        return self.marker
    #def set_samples_per_symbol(self, samples_per_symbol):
    #    pass
    #def set_num_plots(self, num_plots):
    #    pass

class datascope_input_watcher (threading.Thread):
    def __init__ (self, msgq, event_receiver, frame_decim, samples_per_symbol, num_plots, sym_decim, **kwds):
        threading.Thread.__init__ (self, **kwds)
        self.setDaemon (1)
        self.msgq = msgq
        self.event_receiver = event_receiver
        self.frame_decim = frame_decim
        self.samples_per_symbol = samples_per_symbol
        self.num_plots = num_plots
        self.sym_decim = sym_decim
        self.iscan = 0
        self.keep_running = True
        self.skip = 0
        self.totsamp = 0
        self.skip_samples = 0
        self.msg_string = ""
        self.lock = threading.Lock()
        self.start ()
    def set_samples_per_symbol(self, samples_per_symbol):
        with self.lock:
            self.samples_per_symbol = samples_per_symbol
            self.reset()
    def set_num_plots(self, num_plots):
        with self.lock:
            self.num_plots = num_plots
            self.reset()
    def set_sym_decim(self, sym_decim):
        with self.lock:
            self.sym_decim = sym_decim
            self.reset()
    def reset(self):
        self.msg_string = ""
        self.skip_samples = 0
    def run (self):
        # print "datascope_input_watcher: pid = ", os.getpid ()
        print "Num plots: %d, samples per symbol: %d" % (self.num_plots, self.samples_per_symbol)
        while (self.keep_running):
            msg = self.msgq.delete_head()   # blocking read of message queue
            nchan = int(msg.arg1())    # number of channels of data in msg
            nsamples = int(msg.arg2()) # number of samples in each channel
            
            self.totsamp += nsamples
            
            if self.skip_samples >= nsamples:
               self.skip_samples -= nsamples
               continue

            with self.lock:
                self.msg_string += msg.to_string()      # body of the msg as a string
            
                bytes_needed = (self.num_plots*self.samples_per_symbol) * gr.sizeof_float
                if (len(self.msg_string) < bytes_needed):
                    continue
    
                records = []
                # start = self.skip * gr.sizeof_float
                start = 0
                chan_data = self.msg_string[start:start+bytes_needed]
                rec = numpy.fromstring (chan_data, numpy.float32)
                records.append (rec)
                self.msg_string = ""
    
                unused = nsamples - (self.num_plots*self.samples_per_symbol)
                unused -= (start/gr.sizeof_float)
                self.skip = self.samples_per_symbol - (unused % self.samples_per_symbol)
                # print "reclen = %d totsamp %d appended %d skip %d start %d unused %d" % (nsamples, self.totsamp, len(rec), self.skip, start/gr.sizeof_float, unused)
    
                de = datascope_DataEvent (records, self.samples_per_symbol, self.num_plots)
            wx.PostEvent (self.event_receiver, de)
            records = []
            del de

            self.skip_samples = self.num_plots * self.samples_per_symbol * self.sym_decim   # lower values = more frequent plots, but higher CPU usage

class datascope_window(wx.Panel):
    def __init__(self,
                  info,
                  parent,
                  id=-1,
                  samples_per_symbol=10,
                  num_plots=100,
                  sym_decim=20,
                  pos=wx.DefaultPosition,
                  size=wx.DefaultSize,
                  name=""):
        #window_with_ctlbox.__init__ (self, parent, -1)
        wx.Panel.__init__(self, parent, id, pos, size, wx.DEFAULT_FRAME_STYLE, name)
        
        self.info = info

        vbox = wx.BoxSizer(wx.VERTICAL)
        self.graph = datascope_graph_window(
            info,
            self,
            -1,
            samples_per_symbol=samples_per_symbol,
            num_plots=num_plots,
            sym_decim=sym_decim,
            size=size)

        vbox.Add (self.graph, 1, wx.EXPAND)
        #vbox.Add (self.make_control_box(), 0, wx.EXPAND)
        #vbox.Add (self.make_control2_box(), 0, wx.EXPAND)

        self.sizer = vbox
        self.SetSizer (self.sizer)
        self.SetAutoLayout (True)
        self.sizer.Fit (self)
    # second row of control buttons etc. appears BELOW control_box
    #def make_control2_box (self):
    #    ctrlbox = wx.BoxSizer (wx.HORIZONTAL)
    #    ctrlbox.Add ((5,0) ,0) # left margin space
    #    return ctrlbox
    def run_stop (self, evt):
        self.info.running = not self.info.running
    def set_samples_per_symbol(self, samples_per_symbol):
        self.graph.set_samples_per_symbol(samples_per_symbol)
    def set_num_plots(self, num_plots):
        self.graph.set_num_plots(num_plots)
    def set_sym_decim(self, sym_decim):
        self.graph.set_sym_decim(sym_decim)

class datascope_graph_window (plot.PlotCanvas):
    def __init__(self,
            info,
            parent,
            id=-1,
            pos=wx.DefaultPosition,
            size=wx.DefaultSize,    #(140, 140),
            samples_per_symbol=10,
            num_plots=100,
            sym_decim=20,
            style = wx.DEFAULT_FRAME_STYLE,
            name = ""):
        plot.PlotCanvas.__init__ (self, parent, id, pos, size, style, name)

        self.SetXUseScopeTicks (True)
        self.SetEnableGrid (False)
        self.SetEnableZoom (True)
        self.SetEnableLegend(True)
        # self.SetBackgroundColour ('black')
        
        self.info = info;
        self.total_points = 0
        if info.v_scale is not None or info.v_scale == 0:
            self.y_range = (-info.v_scale, info.v_scale) #(-1., 1.)
        else:
            self.y_range = None
        #self.samples_per_symbol = samples_per_symbol
        #self.num_plots = num_plots

        EVT_DATA_EVENT (self, self.format_data)

        self.input_watcher = datascope_input_watcher(
            info.msgq,
            self,
            info.frame_decim,
            samples_per_symbol,
            num_plots,
            sym_decim)
    def set_samples_per_symbol(self, samples_per_symbol):
        self.input_watcher.set_samples_per_symbol(samples_per_symbol)
        #self.samples_per_symbol = samples_per_symbol
    def set_num_plots(self, num_plots):
        self.input_watcher.set_num_plots(num_plots)
        #self.num_plots = num_plots
    def set_sym_decim(self, sym_decim):
        self.input_watcher.set_sym_decim(sym_decim)
    def format_data (self, evt):
        if not self.info.running:
            return
        
        info = self.info
        records = evt.data
        nchannels = len(records)
        npoints = len(records[0])
        self.total_points += npoints

        x_vals = numpy.arange (0, evt.samples_per_symbol)

        self.SetXUseScopeTicks (True)   # use 10 divisions, no labels

        objects = []
        colors = ['red','orange','yellow','green','blue','violet','cyan','magenta','brown','black']

        r = records[0]  # input data
        v_min = None
        v_max = None
        for i in range(evt.num_plots):
            points = []
            for j in range(evt.samples_per_symbol):
                v = r[ i*evt.samples_per_symbol + j ]
                if (v_min is None) or (v < v_min):
                    v_min = v
                if (v_max is None) or (v > v_max):
                    v_max = v
                p = [ j, v ]
                points.append(p)
            objects.append (plot.PolyLine (points, colour=colors[i % len(colors)], legend=('')))

        graphics = plot.PlotGraphics (objects,
                                      title=self.info.title,
                                      xLabel = 'Time', yLabel = 'Amplitude')
        x_range = (0., 0. + (evt.samples_per_symbol-1)) # ranges are tuples!
        if self.y_range is None:
            v_scale = max(abs(v_min), abs(v_max))
            y_range = (-v_scale, v_scale)
            #y_range = (min, max)
        else:
            y_range = self.y_range
        self.Draw (graphics, xAxis=x_range, yAxis=y_range)
