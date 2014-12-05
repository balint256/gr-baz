#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  delayed_start.py
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

import datetime, time

from gnuradio import uhd

class delayed_start():
	def __init__(self, usrp, delay=0, pps=True, base_time=None, offset=0, callbacks=[], gpsdo=False):
		if base_time == float('-inf'):	# Default in GRC
			base_time = None
		
		# FIXME: Check for valid 'base_time'
		if delay <= 0 and base_time is None:
			print "Starting immediately with USRP: %s" % (usrp)
			usrp.start()
			return
		
		print "Delayed start: %f sec with USRP: %s (offset: %f)" % (delay, usrp, offset)
		
		if base_time is not None:
			print "Using base time:", base_time
		
		if gpsdo:
			time_first = usrp.get_time_now().get_real_secs()
		else:
			time_first = time.time()
		time_first_int = int(time_first)
		print "Beginning sync at", time_first_int
		
		while True:
			if gpsdo:
				time_next = usrp.get_time_now().get_real_secs()
			else:
				time_next = time.time()
			time_next_int = int(time_next)
			time_diff = time_next_int - time_first_int
			if time_diff > 0:
				if time_diff == 1:
					#print "Caught local sync at", time_next_int
					
					if offset > 0 and ((time_next - time_first) < offset):
						time_last = time_next
						while True:
							time_next = time.time()
							time_diff = time_next - time_last
							if time_diff >= offset:
								break
							time_last = time_next
					break
				else:
					raise Exception("Failed to sync to local clock")
		
		time_next_int_plus_1 = time_next_int + 1
		
		if base_time is None:
			base_time = time_next_int_plus_1
		
		future_start = base_time + delay
		
		if future_start < time_next:
			raise Exception("Start time is in the past by %f sec" % (future_start - time_next))
		
		uhd_time_next = uhd.time_spec(time_next_int_plus_1)
		if pps:
			if not gpsdo:
				usrp.set_time_next_pps(uhd_time_next)
				print "USRP time set PPS:", time_next_int_plus_1
			else:
				print "Not setting time with GPSDO"
		else:
			usrp.set_time_now(uhd_time_next)
			print "USRP time set NOW:", time_next_int_plus_1
		
		#uhd_base_time = uhd.time_spec(base_time)
		#uhd_stream_time = uhd_base_time + uhd.time_spec(delay)
		uhd_base_time = uhd.time_spec(future_start)
		
		#usrp.set_start_time(uhd_stream_time)
		usrp.set_start_time(uhd_base_time)
		
		for cb in callbacks:
			cb(future_start)
		
		#print "USRP start time set:", future_start
		usrp.start()
		#print "USRP 'started'..."
		print "USRP starting at:", future_start

def main():
	return 0

if __name__ == '__main__':
	main()
