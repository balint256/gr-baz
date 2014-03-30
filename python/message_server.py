#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  message_server.py
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

import threading, traceback, socket, SocketServer, time

from gnuradio import gr, gru

class ThreadedTCPRequestHandler(SocketServer.StreamRequestHandler): # BaseRequestHandler
    # No __init__
    def setup(self):
        SocketServer.StreamRequestHandler.setup(self)
        print "==> Connection from:", self.client_address
        self.request.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, True)
        with self.server.client_lock:
            self.server.clients.append(self)
        self.server.connect_event.set()
        #self.server.command_queue.insert_tail(gr.message_from_string("", -1))
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
                #msg = gr.message_from_string(line, -1)
                #self.server.command_queue.insert_tail(msg)
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

class message_server_thread(threading.Thread):
	def __init__(self, msgq, port, start=True, **kwds):
		threading.Thread.__init__(self, **kwds)
		self.setDaemon(True)
		self.msgq = msgq
		self.keep_running = True
		self.stop_event = threading.Event()
		
		HOST, PORT = "", port   # "localhost"
		print "==> Starting TCP server on port:", port
		while True:
			try:
				self.server = ThreadedTCPServer((HOST, PORT), ThreadedTCPRequestHandler)
				
				self.server.command_queue = msgq
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
		msg = gr.message()  # Empty message to signal end
		self.msgq.insert_tail(msg)
		self.stop_event.wait()
		self.server.shutdown()
		print "Stopped"
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
					with self.server.client_lock:
						for client in self.server.clients:
							try:
								client.wfile.write(msg_str + "\n")
							except socket.error, (e, msg):
								if (e != 32): # Broken pipe
									print "==>", client.client_address, "-", msg
				except Exception, e:
					print e
					traceback.print_exc()
		self.stop_event.set()

class message_server(gr.hier_block2):
	def __init__(self, msgq, port, **kwds):
		gr.hier_block2.__init__(self, "message_server",
								gr.io_signature(0, 0, 0),
								gr.io_signature(0, 0, 0))
		self.thread = message_server_thread(msgq, port, start=False, **kwds)
		self.start()
	def start(self):
		self.thread.start()
	def stop(self):
		self.thread.stop()
	def __del__(self):
		self.stop()

def main():
	return 0

if __name__ == '__main__':
	main()
