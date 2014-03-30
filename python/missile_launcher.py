#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  missile_launcher.py
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

from __future__ import with_statement

import time, math, threading
import usb.core

# sec/degree
_AZIMUTH_RATE = (3.0 / 175.0)
_ELEVATION_RATE = (0.6 / 45.0)
_AZIMUTH_LIMITS = (-135.0,135.0)
_ELEVATION_LIMITS = (-5.0,25.0)
_FIRE_LOCKOUT_TIME = 3.8	# seconds (~3.7, need to wait for turret to rotate)

class missile_launcher:
	def __init__(self, azimuth=0.0, elevation=0.0, threshold=0.0, reset=True):
		#print "==> Movement threshold:", threshold
		self.dev = usb.core.find(idVendor=0x2123, idProduct=0x1010)
		if self.dev is None:
			raise ValueError('Launcher not found.')
		if self.dev.is_kernel_driver_active(0) is True:
			self.dev.detach_kernel_driver(0)
		self.dev.set_configuration()
		
		self.azimuth = 0.0
		self.elevation = 0.0
		self.fire_state = False
		self.threshold = threshold
		self.lockout = None
		self.total_azimuth_travel = 0.0
		self.calibrating = False
		self.cmd_delay = 0.1
		
		if reset:
			self.reset()
		
		self.set_elevation(elevation)
		self.set_azimuth(azimuth)
	
	def reset(self):
		print "==> Resetting turret..."
		self.calibrating = True
		self.set_elevation(-90.0, False, -5.0)
		self.set_azimuth(350.0, False, 151-7) # MAGIC # Calc'd: 141.1
		#self.set_azimuth(-350.0, False, -139.7)
		self.set_elevation(0.0)
		self.set_azimuth(0.0, from_limit=True)
		self.calibrating = False
		self.total_azimuth_travel = 0.0
		print "==> Done."
	
	def get_total_azimuth_travel(self):
		return self.total_azimuth_travel
	
	def set_azimuth(self, azimuth, check=True, actual=None, from_limit=False):
		if math.isnan(azimuth):
			return
		if azimuth == self.azimuth:
			return
		if check:
			azimuth = max(min(azimuth,_AZIMUTH_LIMITS[1]),_AZIMUTH_LIMITS[0])
		diff = azimuth - self.azimuth
		#print "==> Diff", diff
		if abs(diff) < self.threshold:
			return
		if not self._check_lockout():	# Will not fire if moved
			return
		if from_limit:
		#if False:
			if diff > 0.0:
				delay = (abs(diff) - 2.80 - 1.90) / 57.8
				#print "==> From limit CW delay:", delay
			else:
				delay = (abs(diff) - 4.16 - 0.49) / 58.85
				#print "==> From limit CCW delay:", delay
		else:
			#if diff > 0.0:
			#	delay = (abs(diff) - 7.08 + 0.65) / 57.5
			#else:
			#	delay = (abs(diff) - 10.56 + 0.82) / 57.62
			delay = (abs(diff) - 5) / 55
		if delay < 0.15:	# MAGIC
			#print "==> Fixing delay on angle diff", diff
			#delay = 0.0
			#delay = abs(diff) * _AZIMUTH_RATE
			print "==> Skipping angle", diff
			return
		if diff > 0.0:
			#print "==> Right", abs(diff)
			self.turretRight()
		else:
			#print "==> Left", abs(diff)
			self.turretLeft()
		time.sleep(delay)
		self.turretStop()
		if check and actual is None:
			self.total_azimuth_travel += abs(diff)
		
		if actual is None:
			self.azimuth = azimuth
		else:
			self.azimuth = actual
		print "==> Azimuth is now:", self.azimuth

	def set_elevation(self, elevation, check=True, actual=None):
		if math.isnan(elevation):
			return
		if elevation == self.elevation:
			return
		if check:
			elevation = max(min(elevation,_ELEVATION_LIMITS[1]),_ELEVATION_LIMITS[0])
		diff = elevation - self.elevation
		if abs(diff) < self.threshold:
			return
		if not self._check_lockout():	# Will not fire if moved
			return
		if diff > 0.0:
			self.turretUp()
		else:
			self.turretDown()
		time.sleep(abs(diff) * _ELEVATION_RATE)
		self.turretStop()
		
		if actual is None:
			self.elevation = elevation
		else:
			self.elevation = actual
	
	def launch(self, confirm=True, auto_reset=True):
		confirm = bool(confirm)
		if confirm == self.fire_state:
			return
		if confirm and self._check_lockout():
			print "==> Firing!"
			self.turretFire()
			self.lockout = time.time()
		if auto_reset:
			self.fire_state = False
		else:
			self.fire_state = confirm
	
	def is_firing(self):
		return self.fire_state
	
	def is_calibrating(self):
		return self.calibrating
	
	def _check_lockout(self):
		if self.lockout is None:
			return True
		now = time.time()
		if (now - self.lockout) < _FIRE_LOCKOUT_TIME:
			return False
		self.lockout = None
		return True
	
	def turretUp(self):
		self.dev.ctrl_transfer(0x21,0x09,0,0,[0x02,0x02,0x00,0x00,0x00,0x00,0x00,0x00])
		time.sleep(self.cmd_delay)

	def turretDown(self):
		self.dev.ctrl_transfer(0x21,0x09,0,0,[0x02,0x01,0x00,0x00,0x00,0x00,0x00,0x00])
		time.sleep(self.cmd_delay)

	def turretLeft(self):
		self.dev.ctrl_transfer(0x21,0x09,0,0,[0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00])
		time.sleep(self.cmd_delay)

	def turretRight(self):
		self.dev.ctrl_transfer(0x21,0x09,0,0,[0x02,0x08,0x00,0x00,0x00,0x00,0x00,0x00])
		time.sleep(self.cmd_delay)

	def turretStop(self):
		self.dev.ctrl_transfer(0x21,0x09,0,0,[0x02,0x20,0x00,0x00,0x00,0x00,0x00,0x00])
		time.sleep(self.cmd_delay)

	def turretFire(self):
		self.dev.ctrl_transfer(0x21,0x09,0,0,[0x02,0x10,0x00,0x00,0x00,0x00,0x00,0x00])
		time.sleep(self.cmd_delay)

class async_missile_launcher_thread(threading.Thread):
	def __init__(self, ml, streaming=False, recal_threshold=0, **kwds):
		threading.Thread.__init__ (self, **kwds)
		self.setDaemon(1)
		self.active = False
		self.ml = ml
		self.recal_threshold = recal_threshold
		self.q = []
		self.e = threading.Event()
		self.l = threading.Lock()
		self.streaming = streaming
	def get_lock(self):
		return self.l
	def start(self):
		self.active = True
		threading.Thread.start(self)
	def stop(self):
		self.e.set()
		self.active = False
	def run(self):
		while self.active:
			self.e.wait()
			self.e.clear()
			while True:
				evt = None
				with self.l:
					if len(self.q) == 0:
						break
					else:
						evt = self.q.pop()
				#print "<", evt
				evt[0](**evt[1])
				if self.recal_threshold > 0 and self.ml.get_total_azimuth_travel() > self.recal_threshold:
					print "==> Recalibration threshold reached"
					last_el = self.ml.elevation
					self.ml.reset()
					print "==> Restoring last elevation:", last_el
					self.ml.set_elevation(last_el)	# HACK
					if self.streaming:
						with self.l:
							self.q = []
	def post(self, fn, **kwds):
		with self.l:
			item = [(fn, kwds)]
			#print ">", item
			self.q = item + self.q
			self.e.set()
			return True
	def get_queue(self):
		return self.q

class async_missile_launcher():
	def __init__(self, streaming=True, recal_threshold=0, **kwds):
		self._ml = missile_launcher(**kwds)
		self._thread = async_missile_launcher_thread(self._ml, streaming, recal_threshold)
		self.streaming = streaming
		self._thread.start()
	def __del__(self):
		self._thread.stop()
	def _check_queue(self):
		if not self.streaming:
			return True
		with self._thread.get_lock():
			if len(self._thread.get_queue()) > 0:
				#print "==> Skipping"
				return False
		return True
	def set_azimuth(self, azimuth):
		if math.isnan(azimuth):
			return
		if self._check_queue():
			self._thread.post(self._ml.set_azimuth, azimuth=azimuth)
	def set_elevation(self, elevation):
		if math.isnan(elevation):
			return
		if self._check_queue():
			self._thread.post(self._ml.set_elevation, elevation=elevation)
	def launch(self, confirm=True, auto_reset=True):
		confirm = bool(confirm)
		if self.streaming:
			with self._thread.get_lock():
				if confirm == self._ml.is_firing():
					return
		self._thread.post(self._ml.launch, confirm=confirm, auto_reset=auto_reset)
	def is_calibrating(self):
		return self._ml.is_calibrating()
	def calibrate(self, cal=True):
		if cal == False:
			return
		#print "Calibrating..."
		#print "==> Recalibration threshold reached"
		#last_el = self._ml.elevation
		#self._ml.reset()
		#print "==> Restoring last elevation:", last_el
		self._thread.post(self._ml.reset)

def main():
	ml = missile_launcher(reset=False)
	ml.reset()
	# 1
	#ml.set_azimuth(270.0, False, 135.0)
	#ml.turretRight()
	#time.sleep(5)
	#ml.turretStop()
	# 2
	#ml.set_azimuth(-270.0, False, -135.0)
	#ml.turretLeft()
	#time.sleep(5)
	#ml.turretStop()
	# 3
	#ml.set_azimuth(270.0, False, 135.0)
	#ml.turretRight()
	#time.sleep(6)
	#ml.turretStop()
	#time.sleep(1)
	#ml.turretLeft()
	#time.sleep(3)
	#ml.turretStop()
	# 4
	#ml.turretLeft()
	#time.sleep(6)
	#ml.turretStop()
	#time.sleep(1)
	#ml.turretRight()
	#time.sleep(1)
	#ml.turretStop()
	# 5
	#ml.turretRight()
	#time.sleep(6)
	#ml.turretStop()
	#time.sleep(1)
	#ml.turretLeft()
	#time.sleep(1)
	#ml.turretStop()
	#raw_input()
	#ml.turretLeft()
	#time.sleep(3)
	#ml.turretStop()
	# 6
	#ml.turretLeft()
	#time.sleep(6)
	#ml.turretStop()
	#time.sleep(1)
	#ml.turretRight()
	#time.sleep(1)
	#ml.turretStop()
	#raw_input()
	#ml.turretRight()
	#time.sleep(3)
	#ml.turretStop()

def calibrate(ml):
	start = time.time()
	#ml.turretLeft()
	ml.turretUp()
	raw_input()
	ml.turretStop()
	stop = time.time()
	print (stop - start)

if __name__ == '__main__':
	main()
