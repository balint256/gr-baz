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

import sys, socket, traceback, time, datetime, copy, pprint
from optparse import OptionParser

from gnuradio import uhd

try:
	import influxdb
except:
	influxdb = None

def main():
	parser = OptionParser(usage="%prog: [options]")
	
	# FIXME: Add TCP server to avoid mess with pipes
	#parser.add_option("-p", "--port", type="int", default=12345, help="port [default=%default]")
	parser.add_option("-a", "--args", type="string", default="", help="UHD device args [default=%default]")
	parser.add_option("-f", "--fifo", type="string", default="", help="UHD device args [default=%default]")
	parser.add_option("-s", "--silent", default=False, action="store_true", help="Don't print sensor values [default=%default]")
	parser.add_option("--db-host", type="string", default="", help="Database host [default=%default]")
	parser.add_option("--db-id", type="string", default="", help="Database host [default=%default]")
	
	(options, args) = parser.parse_args()
	
	f = None
	if options.fifo != "":
		try:
			f = open(options.fifo, 'w')
		except Exception, e:
			print "Failed to open FIFO:", options.fifo, e

	influxdb_client = None
	if len(options.db_host) > 0:
		if influxdb is not None:
			influxdb_client = influxdb.InfluxDBClient(host=options.db_host, database="gps")
		else:
			raise RuntimeError("influxdb not available")
	
	if options.args == "-":
		usrp = None
	else:
		nmea_sensors = ["gps_gpgga", "gps_gprmc", "gps_servo"]

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

	"""
[date][1PPS Count][Fine DAC][UTC offset ns][Frequency Error Estimate][Sats Visible][Sats Tracked][Lock State][Health Status]
Please see the SYNChronization? command for detailed information on how to decode the health status indicator values.
Note: health status information is available with firmware versions 0.913 and later.
The Lock State variable indicates one of the following states:
0
OCXO warmup
1
Holdover
2
Locking (OCXO training)
4
[Value not defined]
5
Holdover, but still phase locked (stays in this state for about 100s after GPS lock is lost)
6
Locked, and GPS active

HEALTH STATUS |= 0x1;	If the OCXO coarse-DAC is maxed-out at 255
HEALTH STATUS |= 0x2;	If the OCXO coarse-DAC is mined-out at 0
HEALTH STATUS |= 0x4;	If the phase offset to UTC is >250ns
HEALTH STATUS |= 0x8;	If the run-time is < 300 seconds
HEALTH STATUS |= 0x10;	If the GPS is in holdover > 60s
HEALTH STATUS |= 0x20;	If the Frequency Estimate is out of bounds
HEALTH STATUS |= 0x100;	If the short-term-drift (ADEV @ 100s) > 100ns
HEALTH STATUS |= 0x200;	For the first 3 minutes after a phase-reset, or a coarsedac change
	"""
	
	prev_values = {}
	LOCK_STATE = {
		0: "OCXO warmup",
		1: "Holdover",
		2: "Locking (OCXO training)",
		4: "[Value not defined]",
		5: "Holdover, but still phase locked",
		6: "Locked, and GPS active"
	}

	HEALTH_STATUS = {
		0x1: "OCXO coarse-DAC is maxed-out at 255",
		0x2: "OCXO coarse-DAC is mined-out at 0",
		0x4: "phase offset to UTC is >250ns",
		0x8: "run-time is < 300 seconds",
		0x10: "GPS is in holdover > 60s",
		0x20: "Frequency Estimate is out of bounds",
		0x100: "short-term-drift (ADEV @ 100s) > 100ns",
		0x200: "first 3 minutes after a phase-reset, or a coarsedac change"
	}
	HEALTH_STATUS_KEY = {
		0x1: "health_status_coarse_dac_max",
		0x2: "health_status_coarse_dac_min",
		0x4: "health_status_phase_offset",
		0x8: "health_status_run_time",
		0x10: "health_status_holdover",
		0x20: "health_status_freq_estimate",
		0x100: "health_status_short_term_drift",
		0x200: "health_status_phase_reset"
	}
	# HEALTH_STATUS_MASK = 0xF | 0x30 | 0x300
	prev_lock_state = None
	prev_health_status = None

	name, value = "", ""
	buf = ""

	try:
		while True:
			influxdb_points = []

			POINT = {
				'measurement': 'stats',
				'time': datetime.datetime.utcnow(),
				'tags': {
				},
				'fields': {
				}
			}
			FIELDS = POINT['fields']
			TAGS = POINT['tags']

			# if usrp is not None:
			# 	influxdb_points.append(POINT)

			# _POINT = copy.deepcopy(POINT)

			if len(options.db_id) > 0:
				TAGS['id'] = options.db_id

			if usrp is None:
				raw_lines = raw_input()
				lines = (buf + raw_lines).splitlines(True)
				buf = ""
				cnt = 0
				sentences = {}
				for line in lines:
					if line == line.strip():
						buf = "\n".join(lines[cnt:])
						print "Residue: ", buf
						break
					parts = line.split(',')
					sentences[parts[0]] = line.strip()
					cnt += 1
				nmea_sensors = sentences.keys()

			for name in nmea_sensors:
				if usrp is not None:
					if name not in mboard_sensor_names:
						print "Sensor '%s' not available" % (name)
						continue

					try:
						sensor_value = usrp.get_mboard_sensor(name)
					except RuntimeError, e:
						print "Failed to read sensor '{}': {}".format(name, e)
						continue
					value = sensor_value.value.strip()
				else:
					value = sentences[name].strip()

				if value == "":
					continue

				if not options.silent:
					print value

				if name in prev_values and prev_values[name] == value:
					continue

				prev_values[name] = value

# 06-01-01 0 51487 0.00 1.00E-08 0 0 1 0x38
# $GPGGA,020456.00,0000.0000,N,00000.0000,E,0,99,1.0,0.0,M,0.0,M,,*59
# $GPRMC,020456.00,V,0000.0000,N,00000.0000,E,0.0,0.0,010106,,*27

# $GPGGA,182303.00,3745.0818,N,12224.0117,W,2,11,1.0,41.3,M,-29.8,M,,*6B
# $GPRMC,182303.00,A,3745.0818,N,12224.0117,W,0.0,0.0,180317,,*22
# 17-03-18 443237 60512 -9.60 -1.20E-11 13 11 6 0x0

				if name == "gps_gprmc" or name == "$GPRMC":
					idx = value.find('*')
					value = value[0:idx]

					parts = value.split(',')

					time_str = parts[1]
					locked_str = parts[2]
					lat = float(parts[3])
					lat_dir = parts[4]
					lon = float(parts[5])
					lon_dir = parts[6]
					speed = float(parts[7])
					heading = float(parts[8])
					date_str = parts[9]
					# Magnetic variation
					# Direction

					if lat_dir.upper() == "S":
						lat *= -1.0
					if lon_dir.upper() == "W":
						lon *= -1.0

					# time_str
					# date_str

					locked = locked_str.upper() == "A"

					FIELDS['locked'] = locked
					FIELDS['lat'] = lat
					FIELDS['lon'] = lon
					FIELDS['speed'] = speed
					FIELDS['heading'] = heading
				elif name == "gps_gpgga" or name == "$GPGGA":
					idx = value.find('*')
					value = value[0:idx]

					parts = value.split(',')

					time_str = parts[1]
					lat = float(parts[2])
					lat_dir = parts[3]
					lon = float(parts[4])
					lon_dir = parts[5]
					fix = int(parts[6])
					sats = int(parts[7])
					hdop = float(parts[8])
					alt = float(parts[9])
					# Meters
					wgs84_height = float(parts[11])
					# Meters
					# Time since DGPS
					# DGPS station ID

					if lat_dir.upper() == "S":
						lat *= -1.0
					if lon_dir.upper() == "W":
						lon *= -1.0

					FIELDS['fix'] = fix
					FIELDS['sats'] = sats
					FIELDS['hdop'] = hdop
					FIELDS['alt'] = alt
					FIELDS['wgs84_height'] = wgs84_height
				elif name == "gps_servo":
					parts = value.split()

					if len(parts) < 9:
						print "Malformed: {}: {}".format(name, value)
						continue

					date = parts[0]
					pps_count = int(parts[1])
					fine_dac = int(parts[2])
					utc_offset_ns = float(parts[3])
					freq_error = float(parts[4])
					sats_visible = int(parts[5])
					sats_tracked = int(parts[6])
					lock_state = int(parts[7])
					health_status = int(parts[8], 16)

					FIELDS['pps_count'] = pps_count
					FIELDS['fine_dac'] = fine_dac
					FIELDS['utc_offset'] = utc_offset_ns
					FIELDS['freq_error'] = freq_error
					FIELDS['sats_visible'] = sats_visible
					FIELDS['sats_tracked'] = sats_tracked
					FIELDS['lock_state'] = lock_state

					date_day, date_month, date_year = date.split('-')

					if prev_lock_state is None or prev_lock_state != lock_state:
						FIELDS['lock_state_str'] = LOCK_STATE.get(lock_state, '(unknown)')

					prev_lock_state = lock_state

					if prev_health_status is None or prev_health_status != health_status:
						if prev_health_status is not None:
							diff = prev_health_status ^ health_status
							new = health_status & diff
						else:
							new = health_status

						health_statuses = []
						for k,v in HEALTH_STATUS.items():
							if k & new:
								health_statuses.append(v)
						if len(health_statuses) == 0:
							health_statuses.append("OK")
						FIELDS['health_status_str'] = ", ".join(health_statuses)

						for k,v in HEALTH_STATUS.items():
							if k & health_status:
								FIELDS[HEALTH_STATUS_KEY[k]] = True

					prev_health_status = health_status

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

			# if usrp is None:
			if len(FIELDS) > 0:
				influxdb_points.append(POINT)

			if influxdb_client is not None:
				influxdb_client.write_points(influxdb_points)
				# pprint.pprint(influxdb_points)

			if usrp is not None:
				time.sleep(1)
	except KeyboardInterrupt:
		print "Stopping..."
	except Exception, e:
		print "Unhandled exception:", e
		print name, "=", value
		print traceback.format_exc()
	
	if f is not None:
		f.close()

if __name__ == '__main__':
	main()
