#!/usr/bin/env python
# 
# Copyright 2014 Balint Seeber <balint256@gmail.com>
# 
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
# 
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
# 

import time, math

from gnuradio import gr, uhd
import pmt

class usrp_agc(gr.basic_block):
    """
    docstring for block fec_sync
    """
    def __init__(self, u, target_level=0.5, hysteresis=0.6, saturation_level=0.85, adjustment_step=1.0, saturation_backoff_step=20, enabled=True, hold_time=5.0, alpha=0.3, max_adjust=20, post_adjust_ignore=0, verbose=False): # FIXME: , chans=[0]
        gr.basic_block.__init__(self,
            name="usrp_agc",
            in_sig=None,
            out_sig=None)
        
        self.u = u
        self.target_level = target_level
        self.saturation_level = saturation_level
        #self.saturation_level = target_level + (hysteresis / 2.0)
        self.adjustment_step = adjustment_step
        self.hysteresis = hysteresis
        self.enabled = enabled
        self.post_adjust_ignore = post_adjust_ignore
        self.hold_time = hold_time
        self.saturation_backoff_step = saturation_backoff_step
        self.alpha = 0.1
        self.verbose = verbose
        self.max_adjust = max_adjust

        self.adjust_ignore_count = 0
        self.last_adjust_time = None
        self.ave_level = None

        _gain_range = u.get_gain_range()
        self.gain_range = (_gain_range.start(), _gain_range.stop())
        print "[AGC] Gain range:", self.gain_range

        print "[AGC] Enabled:", enabled
        print "[AGC] Target level:", target_level
        print "[AGC] Saturation level:", saturation_level
        print "[AGC] Saturation backoff step:", saturation_backoff_step
        print "[AGC] Adjustment step:", adjustment_step
        print "[AGC] Hysteresis:", hysteresis
        print "[AGC] Alpha:", alpha
        print "[AGC] Hold time:", hold_time
        print "[AGC] Max adjust:", max_adjust

        #print "Saturation level:", self.saturation_level

        self.current_gain = u.get_gain()
        print "[AGC] Starting gain:", self.current_gain

        self.message_port_register_in(pmt.intern('in'))
        self.set_msg_handler(pmt.intern('in'), self.handle_in)

    def general_work(self, input_items, output_items):
    	print "general_work"
    	return 0

    def enable(self, on):
        if on:
            print "[AGC] Enabling"
        else:
            print "[AGC] Disabling"
        self.enabled = on

    def adjust_gain(self, new_gain, relative):
        if not relative:
            if new_gain < self.gain_range[0]:
                print "[AGC] Clipping gain to minimum:", self.gain_range[0], "from", new_gain
                new_gain = self.gain_range[0]
            elif new_gain > self.gain_range[1]:
                print "[AGC] Clipping gain to maximum:", self.gain_range[1], "from", new_gain
                new_gain = self.gain_range[1]
            relative_gain = new_gain - self.current_gain
        else:
            relative_gain = new_gain
            new_gain = self.current_gain + new_gain

            if (relative_gain < 0) and (self.current_gain == self.gain_range[0]):
                print "[AGC] Already at minimum gain:", self.current_gain
            elif (relative_gain > 0) and (self.current_gain == self.gain_range[1]):
                print "[AGC] Already at maximum gain:", self.current_gain

        if self.current_gain == new_gain:
            print "[AGC] New gain is already set:", new_gain
            return new_gain

        print "[AGC] Setting new gain:", new_gain, "changing by:", relative_gain
        self.u.set_gain(new_gain)
        self.current_gain = new_gain
        self.last_adjust_time = time.time()

        self.adjust_ignore_count = self.post_adjust_ignore

        return new_gain

    def handle_in(self, msg):
        old_gain = self.current_gain = self.u.get_gain()

        level_msg = pmt.to_python(msg)
        level = float(level_msg[1])
        target_diff = self.target_level - level

        if self.ave_level is None:
            self.ave_level = level
        else:
            self.ave_level = ((1.0 - self.alpha) * self.ave_level) + (self.alpha * level)
        ave_diff = self.target_level - self.ave_level

        if not self.enabled:
            return

        time_now = time.time()

        time_diff = self.hold_time
        if self.last_adjust_time is not None:
            time_diff = time_now - self.last_adjust_time

    	print "[AGC] Current signal level: %.3f, target diff: %.3f, average: %.3f, average diff: %.3f, gain: %.3f"  % (level, target_diff, self.ave_level, ave_diff, self.current_gain)

        if self.adjust_ignore_count > 0:
            self.adjust_ignore_count -= 1
            if self.verbose: print "[AGC] Ignoring level update, left:", self.adjust_ignore_count
            return

        if level >= self.saturation_level:
            saturation_diff = level - self.saturation_level
            print "[AGC] Level above saturation by:", saturation_diff
            new_gain = self.adjust_gain(-self.saturation_backoff_step, True)
            print "[AGC] Backing off to gain:", new_gain, "by", self.saturation_backoff_step, "from", old_gain

            self.ave_level = None
            #self.adjust_ignore_count = 1

            return
        
        if time_diff < self.hold_time:
            if self.verbose: print "[AGC] Still within hold time by:", (self.hold_time - time_diff)
            return

        if (ave_diff < (-self.hysteresis / 2.0)) or (ave_diff > (self.hysteresis / 2.0)):
            if ave_diff < 0:
                print "[AGC] Average level above target by:", -ave_diff
                target_diff_ratio = level / self.target_level
            else:
                print "[AGC] Average level below target by:", ave_diff
                target_diff_ratio = self.target_level / level

            target_diff_log = math.log10(target_diff_ratio) * 20.0
            target_diff_log *= self.adjustment_step

            if (self.max_adjust is not None) and (target_diff_log > self.max_adjust) and (ave_diff > 0):
                print "[AGC] Constraining jump from:", target_diff_log, "to:", self.max_adjust
                target_diff_log = self.max_adjust

            if ave_diff < 0:
                target_diff_log = -math.ceil(target_diff_log)
            else:
                target_diff_log = math.floor(target_diff_log)

            print "[AGC] Target diff log ratio:", target_diff_log, ", adjustment:", target_diff_log

            self.adjust_gain(target_diff_log, True)

            return
        #
