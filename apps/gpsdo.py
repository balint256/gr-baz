#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  gpsdo.py
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

import sys, socket, traceback, time
from optparse import OptionParser

from gnuradio import uhd

def main():
	parser = OptionParser(usage="%prog: [options]")
	
	# FIXME: Add TCP server to avoid mess with pipes
	#parser.add_option("-p", "--port", type="int", default=12345, help="port [default=%default]")
	parser.add_option("-a", "--args", type="string", default="", help="UHD device args [default=%default]")
	parser.add_option("-f", "--fifo", type="string", default="", help="UHD device args [default=%default]")
	
	(options, args) = parser.parse_args()
	
	f = None
	if options.fifo != "":
		try:
			f = open(options.fifo, 'w')
		except Exception, e:
			print "Failed to open FIFO:", options.fifo, e
	
	usrp = uhd.usrp_source(
		device_addr=options.args,
		stream_args=uhd.stream_args(
			cpu_format="fc32",
			channels=range(1),
		),
	)
	#self.src.set_subdev_spec(self.spec)
	#self.src.set_clock_source(ref, 0)
	#self.src.set_time_source(pps, 0)
	#self.src.set_samp_rate(requested_sample_rate)
	#self.src.set_center_freq(uhd.tune_request(freq, lo_offset), 0)
	#self.src.set_gain(selected_gain_proxy, 0)
	
	mboard_sensor_names = usrp.get_mboard_sensor_names()
	
	#print mboard_sensor_names
	#for name in mboard_sensor_names:
		#sensor_value = usrp.get_mboard_sensor(name)
		#print name, "=", sensor_value
		#print sensor_value.name, "=", sensor_value.value
		#print sensor_value.to_pp_string()
	
	nmea_sensors = ["gps_gpgga", "gps_gprmc"]
	try:
		while True:
			for name in nmea_sensors:
				if name not in mboard_sensor_names:
					print "Sensor '%s' not available" % (name)
					continue
				sensor_value = usrp.get_mboard_sensor(name)
				value = sensor_value.value.strip()
				if value == "":
					continue
				print value
				if f is not None:
					try:
						f.write(value + "\r\n")
						f.flush()
					except IOError, e:
						if e.errno == 32:	# Broken pipe
							f.close()
							print "FIFO consumer broke pipe. Re-opening..."
							f = open(options.fifo, 'w')	# Will block
							break
			time.sleep(1)
	except KeyboardInterrupt:
		print "Stopping..."
	except Exception, e:
		print "Unhandled exception:", e
	
	if f is not None:
		f.close()

if __name__ == '__main__':
	main()
