#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  radar_server.py
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

import threading, traceback, socket, SocketServer, time, base64, sys

from gnuradio import gr, gru

class my_queue():
    def __init__(self):
        self.lock = threading.Lock()
        self.q = []
        self.event = threading.Event()
    def insert_tail(self, msg):
        with self.lock:
            self.q += [msg]
            self.event.set()
    def wait(self):
        self.event.wait()
        self.event.clear()
    def delete_head(self, blocking=True):
        if blocking:
            self.event.wait()
            self.event.clear()
        with self.lock:
            if len(self.q) == 0:
                return None
            msg = self.q[0]
            self.q = self.q[1:]
            return msg
    #def is_empty(self):
    #    pass

class ThreadedTCPRequestHandler(SocketServer.StreamRequestHandler): # BaseRequestHandler
    # No __init__
    def setup(self):
        SocketServer.StreamRequestHandler.setup(self)
        print "==> Connection from:", self.client_address
        self.request.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, True)
        with self.server.client_lock:
            self.server.clients.append(self)
        self.server.connect_event.set()
        #self.server.command_queue.insert_tail("")
    def handle(self):
        buffer = ""
        while True:
            data = ""   # Initialise to nothing so if there's an exception it'll disconnect
            try:
                data = self.request.recv(1024)
            except socket.error, (e, msg):
                if e != 104:    # Connection reset by peer
                    print "==>", self.client_address, "-", msg
            #data = self.rfile.readline().strip()
            if len(data) == 0:
                break
            
            #print "==> Received from", self.client_address, ":", data
            
            #cur_thread = threading.currentThread()
            #response = "%s: %s" % (cur_thread.getName(), data)
            #self.request.send(response)
            
            buffer += data
            lines = buffer.splitlines(True)
            for line in lines:
                if line[-1] != '\n':
                    buffer = line
                    break
                line = line.strip()
                #print "==> Submitting command:", line
                self.server.command_queue.insert_tail(line)
            else:
                buffer = ""
    def finish(self):
        print "==> Disconnection from:", self.client_address
        with self.server.client_lock:
            self.server.clients.remove(self)
            if len(self.server.clients) == 0:
                self.server.connect_event.clear()
        try:
            SocketServer.StreamRequestHandler.finish(self)
        except socket.error, (e, msg):
            if (e != 32): # Broken pipe
                print "==>", self.client_address, "-", msg

class ThreadedTCPServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
    pass

class radar_server_control_thread(threading.Thread):
	def __init__(self, radar, port, start=True, **kwds):
		threading.Thread.__init__(self, **kwds)
		self.setDaemon(True)
		self.keep_running = True
		self.stop_event = threading.Event()
		self.radar = radar
		
		HOST, PORT = "", port   # "localhost"
		print "==> Starting TCP server on port:", port
		while True:
			try:
				self.server = ThreadedTCPServer((HOST, PORT), ThreadedTCPRequestHandler)
				
				self.server.command_queue = my_queue()
				self.server.client_lock = threading.Lock()
				self.server.clients = []
				self.server.connect_event = threading.Event()
				
				ip, port = self.server.server_address
				self.server_thread = threading.Thread(target=self.server.serve_forever)
				self.server_thread.setDaemon(True)
				self.server_thread.start()
			except socket.error, (e, msg):
				print "    Socket error:", msg
				if (e == 98):
					print "    Waiting, then trying again..."
					time.sleep(5)
					continue
			break
		print "==> TCP server running in thread:", self.server_thread.getName()
		
		if start:
			self.start()
	def start(self):
		print "Starting..."
		threading.Thread.start(self)
	def stop(self):
		print "Stopping..."
		self.keep_running = False
		msg = ""  # Empty message to signal end
		self.server.command_queue.insert_tail(msg)
		self.stop_event.wait()
		self.server.shutdown()
		print "Stopped"
	#def __del__(self):
	#	print "DTOR"
	def send_to_clients(self, strMsg):
		strMsg = strMsg.strip("\r\n") + "\n"

		with self.server.client_lock:
			for client in self.server.clients:
				try:
					client.wfile.write(strMsg)
				except socket.error, (e, msg):
					if (e != 32): # Broken pipe
						print "==>", client.client_address, "-", msg
	def run(self):
		#self.server.connect_event.wait()	# Wait for first connection
		
		radar = self.radar
		
		running = False
		freq = None
		freq_start = 4920
		freq_stop = 6100
		freq_step = 5
		interval = 1.0
		
		while self.keep_running:
			try:
				strMsg = None
				
				if not running:
					self.server.command_queue.wait()
				
				while True:
					command = self.server.command_queue.delete_head(False)
					
					if self.keep_running == False:
						break
					
					if command is not None:
						print "  > Processing command: \"%s\"" % (command)
						
						parts = command.split(" ")
						command = parts[0].upper()
						try:
							if command == "FREQ" and len(parts) > 1:
								freq = int(parts[1])
								if freq_start == freq_stop:
									freq_start = freq_stop = freq
								if radar.set_freq(freq):
									radar.clear_radar_queue()
									strMsg = "FREQ " + str(freq)
								else:
									print "Failed to set frequency %d" % (freq)
							
							elif (command == "FIRPWR" or command == "RSSI" or command == "PHEIGHT" or command == "PRSSI" or command == "INBAND") and len(parts) > 1:
								value = int(parts[1])
								#param = command.lower()
								#cmd = param + " " + str(value)
								#radar.write_param(cmd)
								radar.set_param(command, value)
							
							elif command == "STOP":
								running = False
							
							elif command == "START":
								if len(parts) > 1:
									freq_start = int(parts[1])
								if len(parts) > 2:
									freq_stop = int(parts[2])
								if len(parts) > 3:
									freq_step = abs(int(parts[3]))
								if len(parts) > 4:
									interval = float(parts[4])
								running = True
								freq = None
							
							elif command == "QUIT" or command == "EXIT":
								break
						except Exception, e:
							print e
					else:
						break
				
				if running:
					freq_change = False
				
					if freq is not None:
						if freq_start == freq_stop:
							pass
						else:
							if freq_start < freq_stop:
								freq += freq_step
								if freq > freq_stop:
									running = False
							else:
								freq -= freq_step
								if freq < freq_stop:
									running = False
							if running:
								freq_change = True
					else:
						freq = freq_start
						freq_change = True
					
					if running:
						go = True
					
						if freq_change:
							#print "Setting frequency %d" % (freq)
							if radar.set_freq(freq):
								radar.clear_radar_queue()
							else:
								print "Failed to set frequency %d" % (freq)
								go = False
						
						if go:
							time.sleep(interval)
							
							(cnt, data) = radar.read_queue(True)
							
							#print "Queue: %d items (length: %d)" % (cnt, len(data))
							
							#strMsg = "DATA " + str(freq) + " " + str(cnt)# + " " + data	# No longer using this way
							strMsg = "DATA " + str(freq) + " " + base64.b64encode(data)
							
							#if options.progress:
							#	sys.stdout.write(".")
							#	sys.stdout.flush()
					else:
						strMsg = "END"
				
				if strMsg is not None:
					self.send_to_clients(strMsg)
				
				with self.server.client_lock:
					if len(self.server.clients) == 0 and running:
						running = False
			except Exception, e:
				print e
				traceback.print_exc()
		self.stop_event.set()

class radar_error():
	def __init__(self, item):
		self.tsf = item[0]
		self.rssi = ord(item[1])
		self.width = ord(item[2])
		self.type = ord(item[3])
		self.subtype = ord(item[4])
		self.overflow = 0

class radar_server_message_thread(threading.Thread):
	def __init__(self, msgq, fg, detector=None, queue_size=2048, start=True, **kwds):
		threading.Thread.__init__(self, **kwds)
		self.setDaemon(True)
		self.msgq = msgq
		self.fg = fg
		self.detector = detector
		self.keep_running = True
		self.stop_event = threading.Event()
		
		self.reports = []
		self.queue_size = queue_size
		
		if start:
			self.start()
	def start(self):
		print "Starting..."
		threading.Thread.start(self)
	def stop(self):
		print "Stopping..."
		self.keep_running = False
		msg = gr.message()  # Empty message to signal end
		self.msgq.insert_tail(msg)
		self.stop_event.wait()
		self.server.shutdown()
		print "Stopped"
	def clear_radar_queue(self):
		self.reports = []
	def read_queue(self, raw=False, clear=True):
		reports = self.reports
		if clear:
			self.clear_radar_queue()
		if raw:
			return (len(reports), "".join(reports))
		overflows = 0
		l = []
		last = None
		sizeof_radar_error = 4+1+1+1+1
		for report in reports:
			item = struct.unpack("Icccc", report)  # tsf, rssi, width
			
			# Un-initialised memory, so don't bother checking
			#if (ord(item[3]) != 0):
			#    print "First pad byte = %d" % (ord(item[3]))
			#if (ord(item[4]) != 0):
			#    print "Second pad byte = %d" % (ord(item[4]))
			
			re = radar_error(item)
			#print "TSF = %d, RSSI = %d, width = %d" % (re.tsf, re.rssi, re.width)
			l += [re]
			
			if last is not None:
				if (re.tsf < last.tsf):
					overflows += 1
					#print "ROLLOVER: %d (%d)" % (overflows, time_diff)
			re.overflow = overflows
			
			last = re
		
		overflow_amount = (0x7fff + 1)    # 15-bit TSF
		for re in l:
			re.tsf -= (overflow_amount * (overflows - re.overflow))
		
		return l
	#def write_param(self, param):
	#	pass
	def set_param(self, param, value):
		#"FIRPWR"
		#"RSSI"
		#"PHEIGHT"
		#"PRSSI"
		#"INBAND"
		if self.detector is not None:
			self.detector.set_param(param, value)
	def set_freq(self, freq):
		self.fg.set_freq(freq * 1e6)
		return True
	#def __del__(self):
	#	print "DTOR"
	def run(self):
		if self.msgq:
			while self.keep_running:
				msg = self.msgq.delete_head()
				if self.keep_running == False:
					break
				try:
					#msg.type()
					
					msg_str = msg.to_string()
					if len(self.reports) < self.queue_size:
						self.reports += [msg_str]
					#else:
					#	print "RADAR queue full"
				except Exception, e:
					print e
					traceback.print_exc()
		self.stop_event.set()

class radar_server(gr.hier_block2):
	def __init__(self, fg, msgq, detector=None, queue_size=2048, port=5256, **kwds):
		gr.hier_block2.__init__(self, "radar_server",
								gr.io_signature(0, 0, 0),
								gr.io_signature(0, 0, 0))
		self.radar = radar_server_message_thread(msgq, fg, detector, queue_size, start=False, **kwds)
		self.control = radar_server_control_thread(self.radar, port, start=False, **kwds)
		self.start()
	def start(self):
		self.control.start()
		self.radar.start()
	def stop(self):
		self.radar.stop()
		self.control.stop()
	def __del__(self):
		self.stop()

def main():
	return 0

if __name__ == '__main__':
	main()
