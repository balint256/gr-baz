#
# Copyright 2007 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
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
from gnuradio import gr, gru, op25 as _op25
try:
    from gnuradio import fsk4   # LEGACY
except:
    pass
import math

# Reference code
#class decode_watcher(threading.Thread):
#    def __init__(self, msgq, traffic_pane, **kwds):
#        threading.Thread.__init__ (self, **kwds)
#        self.setDaemon(1)
#        self.msgq = msgq
#        self.keep_running = True
#        self.start()
#    def run(self):
#        while(self.keep_running):
#            msg = self.msgq.delete_head()
#            pickled_dict = msg.to_string()
#            attrs = pickle.loads(pickled_dict)
#            #update(attrs)

SYMBOL_DEVIATION = 600
SYMBOL_RATE = 4800

class op25_decoder(gr.hier_block2):
    def __init__(self, channel_rate, auto_tune_msgq=None, defer_creation=False, output_dibits=False, key=None):
        num_outputs = 1
        if output_dibits:
            num_outputs += 1
        
        gr.hier_block2.__init__(self, "op25",
                              gr.io_signature(1, 1, gr.sizeof_float),
                              gr.io_signature(num_outputs, num_outputs, gr.sizeof_float))
        
        self.symbol_rate = SYMBOL_RATE
        
        #print "Channel rate:", channel_rate
        self.channel_rate = channel_rate
        self.auto_tune_msgq = auto_tune_msgq
        self.output_dibits = output_dibits
        self.key = key
        
        if defer_creation == False:
            self.create()
        
    def create(self):
        self.op25_msgq = gr.msg_queue(2)
        self.slicer = None
        try:
            levels = [ -2.0, 0.0, 2.0, 4.0 ]
            self.slicer = _op25.fsk4_slicer_fb(levels)
            self.p25_decoder = _op25.decoder_bf()   # FIXME: Message queue?
        except:
            try:
                self.p25_decoder = _op25.decoder_ff(self.op25_msgq)   # LEGACY
            except:
                raise Exception("Could not find a decoder to use")
        
        # Reference code
        #self.decode_watcher = decode_watcher(self.op25_msgq, self.traffic)
        
        if self.key is not None:
            self.set_key(self.key)
        
        # Reference code
        #trans_width = 12.5e3 / 2;
        #trans_centre = trans_width + (trans_width / 2)
        # discriminator tap doesn't do freq. xlation, FM demodulation, etc.
        #    coeffs = gr.firdes.low_pass(1.0, capture_rate, trans_centre, trans_width, gr.firdes.WIN_HANN)
        #    self.channel_filter = gr.freq_xlating_fir_filter_ccf(channel_decim, coeffs, 0.0, capture_rate)
        #    self.set_channel_offset(0.0, 0, self.spectrum.win._units)
        #    # power squelch
        #    squelch_db = 0
        #    self.squelch = gr.pwr_squelch_cc(squelch_db, 1e-3, 0, True)
        #    self.set_squelch_threshold(squelch_db)
        #    # FM demodulator
        #    fm_demod_gain = channel_rate / (2.0 * pi * self.symbol_deviation)
        #    fm_demod = gr.quadrature_demod_cf(fm_demod_gain)
        # symbol filter        
        #symbol_decim = 1
        #symbol_coeffs = gr.firdes.root_raised_cosine(1.0, channel_rate, self.symbol_rate, 0.2, 500)
        # boxcar coefficients for "integrate and dump" filter
        #samples_per_symbol = channel_rate // self.symbol_rate
        #symbol_duration = float(self.symbol_rate) / channel_rate
        #print "Symbol duration:", symbol_duration
        #print "Samples per symbol:", samples_per_symbol
        #symbol_coeffs = (1.0/samples_per_symbol,)*samples_per_symbol
        #self.symbol_filter = gr.fir_filter_fff(symbol_decim, symbol_coeffs)
        
        # C4FM demodulator
        #print "Symbol rate:", self.symbol_rate
        if self.auto_tune_msgq is None:
            self.auto_tune_msgq = gr.msg_queue(2)
        try:
            self.demod_fsk4 = _op25.fsk4_demod_ff(self.auto_tune_msgq, self.channel_rate, self.symbol_rate)
        except:
            try:
                self.demod_fsk4 = fsk4.demod_ff(self.auto_tune_msgq, self.channel_rate, self.symbol_rate)   # LEGACY
            except:
                raise Exception("Could not find a FSK4 demodulator to use")
        
        # Reference code
        #self.demod_watcher = demod_watcher(autotuneq, self.adjust_channel_offset)
        #list = [[self, self.channel_filter, self.squelch, fm_demod, self.symbol_filter, demod_fsk4, self.p25_decoder, self.sink]]
        
        self.connect(self, self.demod_fsk4)
        if self.slicer:
            self.connect(self.demod_fsk4, self.slicer)
            self.connect(self.slicer, self.p25_decoder)
        else:
            self.connect(self.demod_fsk4, self.p25_decoder)
        self.connect(self.p25_decoder, (self, 0))
        
        if self.output_dibits:
            self.connect(self.demod_fsk4, (self, 1))
    
    def set_key(self, key):
        if type(key) == str:
            if len(key) == 0:
                return
            key = int(key, 16) # Convert from hex string
        self.p25_decoder.set_key(key)
    
    # Reference code
    #def adjust_channel_offset(self, delta_hz):
    #    max_delta_hz = 12000.0
    #    delta_hz *= self.symbol_deviation      
    #    delta_hz = max(delta_hz, -max_delta_hz)
    #    delta_hz = min(delta_hz, max_delta_hz)
    #    self.channel_filter.set_center_freq(self.channel_offset - delta_hz)
