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

_verbose = True

import math
from gnuradio import gr, gru
import op25
from baz import message_callback

fsk4 = None
try:
    from gnuradio import fsk4   # LEGACY
    if _verbose:
        print "[OP25] Imported legacy fsk4"
except:
    pass

SYMBOL_DEVIATION = 600
SYMBOL_RATE = 4800

class op25_fsk4(gr.hier_block2):
    def __init__(self, channel_rate, auto_tune_msgq=None):
        gr.hier_block2.__init__(self, "op25_fsk4",
                              gr.io_signature(1, 1, gr.sizeof_float),
                              gr.io_signature(1, 1, gr.sizeof_float))
        
        self.symbol_rate = SYMBOL_RATE
        
        #print "[OP25] Channel rate:", channel_rate
        self.channel_rate = channel_rate
        self.auto_tune_msgq = auto_tune_msgq
        
        self.auto_tune_message_callback = None
        if self.auto_tune_msgq is None:
            self.auto_tune_msgq = gr.msg_queue(2)
            self.auto_tune_message_callback = message_callback.message_callback(self.auto_tune_msgq)
        
        # C4FM demodulator
        #print "[OP25] Symbol rate:", self.symbol_rate
        try:
            self.demod_fsk4 = op25.fsk4_demod_ff(self.auto_tune_msgq, self.channel_rate, self.symbol_rate)
            if _verbose:
                print "[OP25] Using new fsk4_demod_ff"
        except Exception, e:
            print "[OP25] New fsk4_demod_ff not available:", e
            try:
                self.demod_fsk4 = fsk4.demod_ff(self.auto_tune_msgq, self.channel_rate, self.symbol_rate)   # LEGACY
                if _verbose:
                    print "[OP25] Using legacy fsk4.demod_ff"
            except Exception, e:
                print e
                raise Exception("Could not find a FSK4 demodulator to use")
        
        self.connect(self, self.demod_fsk4, self)

class op25_decoder_simple(gr.hier_block2):
    def __init__(self, idle_silence=True, traffic_msgq=None, key=None, key_map=None, verbose=False):
        gr.hier_block2.__init__(self, "op25_decoder",
                              gr.io_signature(1, 1, gr.sizeof_float),
                              gr.io_signature(1, 1, gr.sizeof_float))
        
        self.traffic_msgq = traffic_msgq
        self.key = key
        self.key_map = key_map
        
        self.traffic_message_callback = None
        
        self.slicer = None
        try:
            levels = [ -2.0, 0.0, 2.0, 4.0 ]
            self.slicer = op25.fsk4_slicer_fb(levels)
            try:
                self.p25_decoder = op25.decoder_bf(idle_silence=idle_silence, verbose=verbose)
            except:
                print "[OP25] Extended decoder_bf constructor not available"
                self.p25_decoder = op25.decoder_bf()    # Original style
            if self.traffic_msgq is not None:
                self.p25_decoder.set_msgq(self.traffic_msgq)
            if _verbose:
                print "[OP25] Using new decoder_bf"
        except Exception, e:
            print "[OP25] New decoder_bf not available:", e
            try:
                if self.traffic_msgq is None:
                    self.traffic_msgq = gr.msg_queue(2)
                    self.traffic_message_callback = message_callback.message_callback(self.traffic_msgq)
                self.p25_decoder = op25.decoder_ff(self.traffic_msgq)   # LEGACY
                if _verbose:
                    print "[OP25] Using legacy decoder_ff"
            except Exception, e:
                print e
                raise Exception("Could not find a decoder to use")

        self.set_key_map(self.key_map)
        
        self.set_key(self.key)
        
        if self.slicer:
            self.connect(self, self.slicer, self.p25_decoder)
        else:
            self.connect(self, self.p25_decoder)
        self.connect(self.p25_decoder, self)
    
    def set_key(self, key):
        try:
            if not hasattr(self.p25_decoder, 'set_key'):
                print "[OP25] This version of the OP25 decoder does not support decryption"
                return False

            l = self._convert_key_string(key)
            if l is None:
                return False
            
            self.p25_decoder.set_key(l)
            return True
        except Exception, e:
            print "[OP25] Exception while setting key:", e
            return False

    def set_key_map(self, key_map):
        try:
            if not hasattr(self.p25_decoder, 'set_key_map'):
                print "[OP25] This version of the OP25 decoder does not support decryption"
                return False

            if (key_map is None) or (len(self.key_map) == 0):
                return False

            _key_map = {}
            for k in key_map.keys():
                v = key_map[k]
                l = self._convert_key_string(v, k)
                if l is None:
                    continue
                _key_map[k] = l
            
            self.p25_decoder.set_key_map(_key_map)
            return True
        except Exception, e:
            print "[OP25] Exception while setting key map:", e
            return False

    def _convert_key_string(self, key, kid=None):
        if key is None:
            return

        kid_str = ""
        if kid is not None:
            kid_str = " 0x%04x" % (kid)

        if type(key) != str:
            print "[OP25] Key%s type %s unsupported: %s" % (kid_str, str(type(key)), str(key))
            return

        if len(key) == 0:
            #print "[OP25] Key%s empty" % (kid_str)
            return

        if (len(key) % 2) == 1:
            print "[OP25] Missing a nibble from key%s: %s" % (kid_str, str(key))
            return

        l = []
        for i in range(len(key) / 2):
            l += [int(key[i*2 + 0] + key[i*2 + 1], 16)]
        return l

    def set_idle_silence(self, on):
        self.p25_decoder.set_idle_silence(on)

    def set_logging(self, on):
        self.p25_decoder.set_logging(on)

class op25_decoder(gr.hier_block2):
    def __init__(self, channel_rate, idle_silence=True, auto_tune_msgq=None, defer_creation=False, output_dibits=False, key=None, key_map=None, traffic_msgq=None, verbose=False):
        num_outputs = 1
        if output_dibits:
            num_outputs += 1
        
        gr.hier_block2.__init__(self, "op25",
                              gr.io_signature(1, 1, gr.sizeof_float),
                              gr.io_signature(num_outputs, num_outputs, gr.sizeof_float))
        
        #self.symbol_rate = SYMBOL_RATE
        
        #print "Channel rate:", channel_rate
        self.channel_rate = channel_rate
        self.idle_silence = idle_silence
        self.verbose = verbose
        self.auto_tune_msgq = auto_tune_msgq
        self.traffic_msgq = traffic_msgq
        self.output_dibits = output_dibits
        self.key = key
        self.key_map = key_map
        
        if defer_creation == False:
            self.create()
    
    def create(self):
        self.fsk4 = op25_fsk4(channel_rate=self.channel_rate, auto_tune_msgq=self.auto_tune_msgq)
        self.decoder = op25_decoder_simple(idle_silence=self.idle_silence, traffic_msgq=self.traffic_msgq, key=self.key, key_map=self.key_map, verbose=self.verbose)
        
        # Reference code
        #self.decode_watcher = decode_watcher(self.op25_msgq, self.traffic)
        
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
        
        # Reference code
        #self.demod_watcher = demod_watcher(autotuneq, self.adjust_channel_offset)
        #list = [[self, self.channel_filter, self.squelch, fm_demod, self.symbol_filter, demod_fsk4, self.p25_decoder, self.sink]]
        
        self.connect(self, self.fsk4, self.decoder, (self, 0))
        
        if self.output_dibits:
            self.connect(self.fsk4, (self, 1))
    
    def set_key(self, key):
        return self.decoder.set_key(key)

    def set_key_map(self, key_map):
        return self.decoder.set_key_map(key_map)

    def set_idle_silence(self, on):
        self.decoder.set_idle_silence(on)

    def set_logging(self, on):
        self.decoder.set_logging(on)
    
    # Reference code
    #def adjust_channel_offset(self, delta_hz):
    #    max_delta_hz = 12000.0
    #    delta_hz *= self.symbol_deviation      
    #    delta_hz = max(delta_hz, -max_delta_hz)
    #    delta_hz = min(delta_hz, max_delta_hz)
    #    self.channel_filter.set_center_freq(self.channel_offset - delta_hz)
