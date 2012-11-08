#!/usr/bin/env python

import threading, traceback, sys, time

class _flusher(threading.Thread):
    def __init__(self, **kwds):
        threading.Thread.__init__(self, **kwds)
        self.setDaemon(True)
        self.keep_running = True
        sys.stderr.write("Starting std flusher...\n")
        self.start()
    def run(self):
        while self.keep_running:
          sys.stdout.flush()
          sys.stderr.flush()
          time.sleep(0.5)

_the_flusher = _flusher()
