#!/usr/bin/env python

import threading, traceback, sys

from gnuradio import gr

_valid_parts = ['msg', 'type', 'arg1', 'arg2', 'length']

class queue_watcher(threading.Thread):
    def __init__(self, msgq,  callback, msg_parts, print_stdout, **kwds):
        threading.Thread.__init__(self, **kwds)
        self.setDaemon(True)
        self.msgq = msgq
        self.callback = callback
        self.msg_parts = msg_parts
        self.print_stdout = print_stdout
        self.keep_running = True
        self.start()
    def run(self):
        while self.msgq and self.keep_running:
            msg = self.msgq.delete_head()
            if self.keep_running == False:
                break
            if self.print_stdout:
                print msg.to_string()
            if self.callback:
                try:
                    to_eval = "self.callback(" + ",".join(map(lambda x: "msg." + x + "()", self.msg_parts)) + ")"
                    try:
                        eval(to_eval)
                    except Exception, e:
                        sys.stderr.write("Exception while evaluating:\n" + to_eval + "\n" + str(e) + "\n")
                        traceback.print_exc(None, sys.stderr)
                except Exception, e:
                    sys.stderr.write("Exception while forming call string using parts: " + str(self.msg_parts) + "\n" + str(e) + "\n")
                    traceback.print_exc(None, sys.stderr)

class message_callback():
    def __init__(self, msgq, callback=None, msg_part='arg1', custom_parts="", dummy=False, consume_dummy=False, print_stdout=False):
        if msgq is None:
            raise Exception, "message_callback requires a message queue"
        self._msgq = msgq
        self._watcher = None
        if dummy and consume_dummy == False:
            #print "[Message Callback] Dummy mode"
            return
        #print "[Message Callback] Starting..."
        if msg_part in _valid_parts:
            msg_parts = [msg_part]
        else:
            msg_parts = filter(lambda x: x in _valid_parts, map(lambda x: x.strip(), custom_parts.split(',')))
        self._watcher = queue_watcher(msgq, callback, msg_parts, print_stdout)
    def __del__(self):
        self.stop()
    def stop(self):
        if self._watcher is None:
            return
        self._watcher.keep_running = False
        msg = gr.message()  # Empty message to signal end
        self._msgq.insert_tail(msg)
    def msgq(self):
        return self._msgq
