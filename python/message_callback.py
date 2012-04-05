#!/usr/bin/env python

import threading

from gnuradio import gr

class queue_watcher(threading.Thread):
    def __init__(self, msgq,  callback, **kwds):
        threading.Thread.__init__ (self, **kwds)
        self.setDaemon(1)
        self.msgq = msgq
        self.callback = callback
        self.keep_running = True
        self.start()
    def run(self):
        while (self.keep_running):
            msg = self.msgq.delete_head()
            self.callback(msg.arg1())

class message_callback():
	def __init__(self, msgq, callback):
		self.watcher = queue_watcher(msgq, callback)
