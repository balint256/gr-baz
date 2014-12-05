#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  pps_diff.py
#  
#  Copyright 2014 Balint Seeber <balint256@gmail.com>
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

import time, datetime
from optparse import OptionParser
from gnuradio import uhd

def main():
	parser = OptionParser(usage="%prog: [options]")
	
	parser.add_option("-a", "--args", type="string", default="", help="UHD device args [default=%default]")
	parser.add_option("-t", "--timeout", type="float", default="2.0", help="PPS timeout [default=%default]")
	parser.add_option("-w", "--wait", type="float", default="0.0", help="PPS timeout [default=%default]")
	parser.add_option("-e", "--epsilon", type="float", default="0.001", help="tolerance [default=%default]")
	parser.add_option("-r", "--ref", type="string", default="external", help="reference selection [default=%default]")
	parser.add_option("-s", "--sensor", type="string", default="ref_locked", help="reference lock sensor name [default=%default]")
	parser.add_option("-l", "--lock-time", type="float", default="0.1", help="reference lock wait time [default=%default]")
	parser.add_option("-L", "--lock-timeout", type="float", default="5.0", help="reference lock wait time [default=%default]")
	
	(options, args) = parser.parse_args()
	
	wall_clock_offsets = []
	ticks_diffs = []
	
	try:
		usrp = uhd.usrp_source(
				device_addr=options.args,
				stream_args=uhd.stream_args(
					cpu_format="fc32",	# Fixed for finite_acquisition
					channels=range(1),
				),
			)
		
		print
		
		master_clock_rate = usrp.get_clock_rate()
		print "Master clock rate:", master_clock_rate
		
		print
		
		usrp.set_clock_source(options.ref)
		usrp.set_time_source(options.ref)
		
		if options.lock_time > 0:
			print "Sleeping for lock..."
			time.sleep(options.lock_time)
		
		sensor_names = usrp.get_mboard_sensor_names()
		if options.sensor in sensor_names:
			sensor_time_start = time.time()
			while True:
				sensor_value = usrp.get_mboard_sensor(options.sensor)
				if sensor_value.to_bool():
					print "Locked"
					break
				if (time.time() - sensor_time_start) > options.lock_timeout:
					print "Sensor '%s' would not indiciate lock within timeout" % (options.sensor)
					return
		else:
			print "Sensor '%s' not found" % (options.sensor)
		
		print
		
		uhd_time_first = usrp.get_time_last_pps()
		print "First last PPS:", float(uhd_time_first.get_real_secs())
		
		print "Waiting for next PPS..."
		wall_clock_last = time.time()
		
		while True:
			uhd_time_last = usrp.get_time_last_pps()
			#if uhd_time_last > uhd_time_first:
			first_step = float(uhd_time_last.get_real_secs()) - float(uhd_time_first.get_real_secs())	# '-' operator not available for time_spec_t in Python/pre-GR-3.7
			if first_step > options.epsilon:	# difference should be 0 throughout rest of first second
				#if first_step != 1.0:
				if abs(first_step - 1.0) > options.epsilon:
					print "Illegal first PPS step:", first_step
					print "First time:", float(uhd_time_first.get_real_secs())
					print "Last time:", float(uhd_time_last.get_real_secs())
					print "Difference:", abs(first_step - 1.0), options.epsilon
					uhd_time_last = None
					break
				
				wall_clock_last = time.time()
				
				print "Caught PPS"
				#print "First:", uhd_time_first.get_full_secs(), uhd_time_first.get_frac_secs()
				first_ticks = uhd_time_first.to_ticks(master_clock_rate)
				print "First ticks:", first_ticks
				#print "Last:", uhd_time_last.get_full_secs(), uhd_time_last.get_frac_secs()
				last_ticks = uhd_time_last.to_ticks(master_clock_rate)
				print "Last ticks:", last_ticks	# Since reference has changed, this won't be precisely master clock rate if reference signals are valid & sync'd
				ticks_diff = last_ticks - first_ticks
				print "Tick diff:", ticks_diff
				print "Diff time:", 1.*ticks_diff / master_clock_rate
				
				break
			
			if options.wait > 0.0:
				time.sleep(options.wait)
			
			if (time.time() - wall_clock_last) > options.timeout:
				print "Failed to catch PPS"
				uhd_time_last = None
				break
		
		print
		
		while uhd_time_last is not None:
			uhd_time_now = usrp.get_time_last_pps()
			step = float(uhd_time_now.get_real_secs()) - float(uhd_time_last.get_real_secs())
			
			#if uhd_time_now == uhd_time_last:
			if step < options.epsilon:
				if options.sensor in sensor_names:
					sensor_value = usrp.get_mboard_sensor(options.sensor)
					if not sensor_value.to_bool():
						print "Lost lock"
						break
				
				if options.wait > 0.0:
					time.sleep(options.wait)
				continue
			
			#if first_step != 1.0:
			if abs(first_step - 1.0) > options.epsilon:
				print "Illegal PPS step:", step
				break
			
			wall_clock_now = time.time()
			wall_clock_diff = wall_clock_now - wall_clock_last
			
			now_ticks = uhd_time_now.to_ticks(master_clock_rate)
			last_ticks = uhd_time_last.to_ticks(master_clock_rate)
			ticks_diff = now_ticks - last_ticks
			
			# Properly NTP sync'd host should have some 0's after WC decimal point
			# Fractional part of WC time is latency in retrieving USRP last_pps time
			print "Wall clock now: %f (diff: %f), ticks diff: %d" % (wall_clock_now, wall_clock_diff, ticks_diff)
			
			ticks_diffs += [1.0*ticks_diff - master_clock_rate]
			
			wall_clock_offset = wall_clock_now - int(wall_clock_now)
			wall_clock_offsets += [wall_clock_offset]
			
			wall_clock_last = wall_clock_now
			uhd_time_last = uhd_time_now
	except KeyboardInterrupt:
		pass
	except Exception, e:
		print "Unhandled exception:", e
	
	print
	
	if len(wall_clock_offsets) > 0:
		ave_wall_clock_offset = sum(wall_clock_offsets) / len(wall_clock_offsets)
		print "Ave wall clock offset:", ave_wall_clock_offset
	
	if len(ticks_diffs) > 0:
		ave_ticks_diff = 1.*sum(ticks_diffs) / len(ticks_diffs)
		print "Ave ticks diff from master clock rate:", ave_ticks_diff
		print "Ave ticks diff (s):", (ave_ticks_diff / master_clock_rate)
	
	return 0

if __name__ == '__main__':
	main()
