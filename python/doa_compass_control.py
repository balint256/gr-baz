from doa_compass_plotter import compass_plotter
from gnuradio.wxgui import forms
from gnuradio.gr import pubsub
import wx

########################################################################
# Controller keys
########################################################################
BEAM_AZM_KEY = 'beam_azm'
BEAM_ENB_KEY = 'beam_enb'

########################################################################
# Constants
########################################################################
POINTER_WIDTH = 3 #degrees
SLIDER_STEP_SIZE = 3 #degrees
BEAM_COLOR_SPEC = (0, 0, 1)
PLOTTER_SIZE = (450, 450)

########################################################################
# Main controller GUI
########################################################################
class compass_control(pubsub.pubsub, wx.Panel):
    def __init__(self, parent, ps=None, direction_key='__direction_key__', callback=None, direction=None, text=None, text_visible=None):
        #init
        if ps is None: ps = pubsub.pubsub()
        if direction is not None: ps[direction_key] = direction
        pubsub.pubsub.__init__(self)
        wx.Panel.__init__(self, parent)
        #proxy keys
        self.proxy(BEAM_AZM_KEY, ps, direction_key)
        #build gui and add plotter
        vbox = wx.BoxSizer(wx.VERTICAL)
        self.plotter = compass_plotter(self)
        self.plotter.SetSize(wx.Size(*PLOTTER_SIZE))
        self.plotter.SetSizeHints(*PLOTTER_SIZE)
        vbox.Add(self.plotter, 1, wx.EXPAND) # | wx.SHAPED #keep aspect ratio
        #build the control box
        #beam_box = forms.static_box_sizer(
            #parent=self,
            #label='Beam Control',
            #bold=True,
        #)
        #vbox.Add(beam_box, 0, wx.EXPAND)
        #beam_box.Add(_beam_control(self, label='Beam', azm_key=BEAM_AZM_KEY, enb_key=BEAM_ENB_KEY), 0, wx.EXPAND)
        #beam_box.Add(_beam_control(self, label='Null', azm_key=NULL_AZM_KEY, enb_key=NULL_ENB_KEY), 0, wx.EXPAND)
        #self[BEAM_ENB_KEY] = True
        #self[NULL_ENB_KEY] = True
        #self[PATTERN_KEY] = 'ant0'
        #forms.drop_down(
            #label='Pattern',
            #ps=self,
            #key=PATTERN_KEY,
            #choices=PATTERN_KEYS,
            #labels=PATTERN_NAMES,
            #sizer=beam_box,
            #parent=self,
            #proportion=0,
        #)
        self.SetSizerAndFit(vbox)
        #subscribe keys to the update methods
        self.subscribe(BEAM_AZM_KEY, self.update)
        
        #self.update_enables() #initial updates
        self.set_direction(direction)
        self.plotter.set_text(text)
        self.plotter.set_text_visible(text_visible, True)

        #do last as to not force initial update
        if callback: self.subscribe(TAPS_KEY, callback)

    def update_enables(self, *args):
        """
        Called when the pattern is changed. 
        Update the beam and null enable states based on the pattern.
        """
        for key, name, beam_enb, null_enb in PATTERNS:
            if self[PATTERN_KEY] != key: continue
            self[BEAM_ENB_KEY] = beam_enb
            self[NULL_ENB_KEY] = null_enb
            self.update()
            return

    def update(self, *args):
        """
        """
        #update the null and beam arrows
        for (enb_key, azm_key, color_spec) in (
            (BEAM_ENB_KEY, BEAM_AZM_KEY, BEAM_COLOR_SPEC),
        ):
            profile = self[enb_key] and [
                (0, self[azm_key]),
                (1.0, self[azm_key]-POINTER_WIDTH/2.0),
                (1.0, self[azm_key]+POINTER_WIDTH/2.0),
            ] or []
            self.plotter.set_profile(
                key='1'+azm_key, color_spec=color_spec,
                fill=True, profile=profile,
            )
        self.plotter.update()
    
    def set_direction(self, direction):
        if direction is None:
            self[BEAM_ENB_KEY] = False
        else:
            self[BEAM_ENB_KEY] = True
            self[BEAM_AZM_KEY] = direction
        self.update()
    
    def set_text(self, text):
        self.plotter.set_text(text)

    def set_text_visible(self, visible):
        self.plotter.set_text_visible(visible)
