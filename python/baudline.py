#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  baudline.py
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

# le32f	- 1 Msps
# le16	- 4 Msps
# Pipe mode kill works, FIFO doesn't

import sys, subprocess, tempfile, os, signal

from gnuradio import gr, gru, blocks

class baudline_sink(gr.hier_block2):
	def __init__(self, fmt, item_size, channels, is_complex, sample_rate, aggregate_channel_count=1,
		flip_complex=False, baseband_freq=None, decimation=1, scale=1.0, overlap=None, slide_size=None, fft_size=None, jump_step=None, x_slip=None,
		mode='pipe', buffered=True, kill_on_del=True, memory=None, peak_hold=False, **kwds):
		
		gr.hier_block2.__init__(self, "baudline_sink",
								gr.io_signature(1, 1, item_size),
								gr.io_signature(0, 0, 0))
		
		baudline_path = gr.prefs().get_string('baudline', 'path', 'baudline')
		
		#tf = tempfile.NamedTemporaryFile(delete=False)
		#tf.write(gp)
		#tf.close()
		#print tf.name
		
		self.mode = mode
		self.kill_on_del = kill_on_del
		
		if mode == 'fifo':
			fifo_name = 'baudline_fifo'
			self.tmpdir = tempfile.mkdtemp()
			self.filename = os.path.join(self.tmpdir, fifo_name)
			print self.filename
			try:
				os.mkfifo(self.filename)
			except OSError, e:
				print "Failed to create FIFO: %s" % e
				raise
		
		baudline_exec = [
			baudline_path,
			"-stdin",
			"-record",
			"-spacebar", "recordpause",
			"-samplerate", str(int(sample_rate)),
			"-channels", str(channels * aggregate_channel_count),
			"-format", fmt,
			#"-backingstore",
			
			# #
			#"-threads",
			#"-pipeline",
			#"-memory",	# MB
			#"-verticalsync"
			
			#"-realtime",
			#"-psd"
			#"-reversetimeaxis",
			#"-overclock",
			
			#"-debug",
			#"-debugtimer", str(1000)
			#"-debugfragments",
			#"-debugcadence",
			#"-debugjitter",
			#"-debugrate",
			#"-debugmeasure
		]
		
		if is_complex:
			baudline_exec += ["-quadrature"]
		if flip_complex:
			baudline_exec += ["-flipcomplex"]
		if baseband_freq is not None and baseband_freq > 0:
			baudline_exec += ["-basefrequency", str(baseband_freq)]
		if decimation > 1:
			baudline_exec += ["-decimateby", str(decimation)]
		if scale != 1.0:
			baudline_exec += ["-scaleby", str(scale)]
		if overlap is not None and overlap > 0:
			baudline_exec += ["-overlap", str(overlap)]
			#"-slidesize"
		if slide_size is not None and slide_size > 0:
			baudline_exec += ["-slidesize", str(slide_size)]
		if fft_size is not None and fft_size > 0:
			baudline_exec += ["-fftsize", str(fft_size)]
		if jump_step is not None and jump_step > 0:
			baudline_exec += ["-jumpstep", str(jump_step)]
		if x_slip is not None and x_slip > 0:
			baudline_exec += ["-xslip", str(x_slip)]
		if memory is not None and memory > 0:
			baudline_exec += ["-memory", str(memory)]
		if peak_hold:
			baudline_exec += ["-peakhold"]
		
		for k in kwds.keys():
			arg = str(k).strip()
			if arg[0] != '-':
				arg = "-" + arg
			baudline_exec += [arg]
			val = kwds[k]
			if val is not None:
				val = str(val).strip()
				if val.find(' ') > -1 and len(val) > 1:
					if val[0] != '\"':
						val = "\"" + val
					if val[-1] != '\"':
						val += "\""
				baudline_exec += [val]

		if mode == 'fifo':
			baudline_exec += ["<", self.filename]
			#baudline_exec = ["cat", self.filename, "|"] + baudline_exec
		
			baudline_exec = [" ".join(baudline_exec)]
		
		self.p = None
		#res = 0
		try:
			#res = subprocess.call(gp_exec)
			print baudline_exec
			if mode == 'pipe':
				self.p = subprocess.Popen(baudline_exec, stdin=subprocess.PIPE)	# , stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=16384 or -1
			elif mode == 'fifo':
				self.p = subprocess.Popen(baudline_exec, shell=True)
			#self.p.communicate(input=)
			
			#self.p.stdin.write()
			#self.p.wait()
		#except KeyboardInterrupt:
		#	print "Caught CTRL+C"
		except Exception, e:
			print e
			raise
		#if self.p is not None and not self.p.returncode == 0:
		#	print "Failed to run subprocess (result: %d)" % (self.p.returncode)
		#if res != 0:
		#	print "Failed to run subprocess (result: %d)" % (res)
		
		if mode == 'pipe':
			print "==> Using FD:", self.p.stdin.fileno()
			self.file_sink = blocks.file_descriptor_sink(item_size, self.p.stdin.fileno())	# os.dup
		elif mode == 'fifo':
			self.file_sink = blocks.file_sink(item_size, self.filename)	# os.dup
			self.file_sink.set_unbuffered(not buffered)	# Flowgraph won't die if baudline exits
		
		self.connect(self, self.file_sink)
		
	def __del__(self):
		#os.unlink(tf.name)
		
		if self.p is not None:	# Won't work in FIFO mode as it blocks
			if self.kill_on_del:
				print "==> Killing baudline..."
				#self.p.kill()
				#self.p.terminate()
				os.kill(self.p.pid, signal.SIGTERM)
		
		if self.mode == 'fifo':
			try:
				print "==> Deleting:", self.filename
				os.unlink(self.filename)
				os.rmdir(self.tmpdir)
			except OSError, e:
				print "Failed to delete FIFO: %s" % e
				raise

def main():
	
	return 0

if __name__ == '__main__':
	main()
