#!/usr/bin/env python

"""
BorIP Server
By Balint Seeber
Part of gr-baz (http://wiki.spench.net/wiki/gr-baz)
Protocol specification: http://wiki.spench.net/wiki/BorIP
"""

from __future__ import with_statement

import time, os, sys, threading, thread, traceback  # , gc
from string import split, join
from optparse import OptionParser
import socket
import threading
import SocketServer

from gnuradio import gr
import baz

class server(gr.hier_block2):   # Stand-alone block
    def __init__(self, size=gr.sizeof_gr_complex, mul=1, server=False, parent=None, verbose=False, debug=False, show_commands=False):    # 'parent' should be flowgraph in which this is the 'BorIP Sink' block
        gr.hier_block2.__init__(self, "borip_server",
            gr.io_signature(1, 1, size),
            gr.io_signature(0, 0, 0))
        
        src = self
        udp_type_size = gr.sizeof_short
        if size == gr.sizeof_gr_complex:
            if mul != 1:
                if verbose:
                    print "**> Applying constant multiplication:", mul
                try: self.mul = blocks.multiply_const_cc(mul)
                except: self.mul = gr.multiply_const_cc(mul)
                self.connect(src, self.mul)
                src = self.mul
            
            self.c2is = gr.complex_to_interleaved_short()
            self.connect(src, self.c2is)
            src = self.c2is
        elif (size == (2 * gr.sizeof_short)):   # Short * 2 (vlen = 2)
            udp_type_size = gr.sizeof_short * 2
        elif not (size == gr.sizeof_short):     # not IShort
            raise Exception, "Invalid input size (must be gr_complex or interleaved short)"
        
        self.sink = baz.udp_sink(udp_type_size, None, 28888, 1024, False, True)
        
        self.connect(src, self.sink)
        
        self.verbose = verbose
        
        self.parent = None
        if server:
            self.parent = parent
            self.server = borip_server(block=self)
            self.server.options.verbose = verbose
            self.server.options.debug = debug
            self.server.options.command = show_commands
            self.server.options.fixed_flowgraph = True
            self.server.options.lock = True
            self.server.start()
    
    def set_status_msgq(self, msgq):
        self.sink.set_status_msgq(msgq)

class ServerWrapper(gr.top_block):  # Wrapper for hier_block with output pad
    def __init__(self, flowgraph, options):
        gr.top_block.__init__(self, "ServerWrapper for " + flowgraph.name())
        
        self.flowgraph = flowgraph
        self.options = options
        
        mul = getattr(flowgraph, 'rescale', 1.0)
        if hasattr(mul, '__call__'):
            mul = mul()
            
        self.server = server(size=self.flowgraph.output_signature().sizeof_stream_item(0), mul=mul, server=False, verbose=options.verbose)
        
        self.connect((self.flowgraph, 0), (self.server, 0))
        
        self.sink = self.server.sink
        
        if hasattr(self.flowgraph, 'source'):
            if options.verbose:
                print "**> Using internal source:", self.flowgraph.source
            self.source = self.flowgraph.source
        
        if hasattr(self.flowgraph, 'status') and hasattr(self.flowgraph.status, 'msgq'):
            #if isinstance(self.flowgraph.status, message_callback.message_callback):   # Doesn't work, need to 'import baz' and prepend 'baz.'
                if options.verbose:
                    print "**> Using status msgq from:", self.flowgraph.status
                
                if hasattr(self.flowgraph.status.msgq, '__call__'):
                    msgq = self.flowgraph.status.msgq()
                else:
                    msgq = self.flowgraph.status.msgq
                    
                self.server.sink.set_status_msgq(msgq)

def _default_device_hint_mapper(hint):
    if hint is None or len(hint) == 0 or hint == "-":
        return "usrp_uhd"
    parts = hint.split(" ")
    try:
        idx = int(parts[0])
        return "usrp_legacy"
    except:
        pass
    try:
        kv = parts[0].index('=')    # key=value
        parts = hint.split(",")		# Assumes no use of "
        args = []
        subdev = None
        for part in parts:
            part = part.strip()
            if len(part) == 1 and part[0].lower() >= 'a' and part[0].lower() <= 'z':
                subdev = part + ":0"
                continue
            idx = part.find(':')
            if idx > 0 and part[0].lower() >= 'a' and part[0].lower() <= 'z':	# "?:"
                subdev = part
                continue
            args += [part]
        combined = ["addr=\"" + ",".join(args) + "\""]
        if subdev is not None:
            combined += ["subdev=\"" + subdev + "\""]
        return {'module': "usrp_uhd", 'args': combined}
    except:
        pass
    return parts[0].upper() # FCD, RTL

_device_hint_mapper = _default_device_hint_mapper

try:
    import borip_server_devmap
    _device_hint_mapper = borip_server_devmap.device_hint_mapper
except:
    pass

class TuneResult():
    def __init__(self, target_rf_freq=0.0, actual_rf_freq=0.0, target_dsp_freq=0.0, actual_dsp_freq=0.0):
        self.target_rf_freq = target_rf_freq
        self.actual_rf_freq = actual_rf_freq
        self.target_dsp_freq = target_dsp_freq
        self.actual_dsp_freq = actual_dsp_freq
    def __str__(self):
        return str(self.target_rf_freq) + ": " + str(self.actual_rf_freq) + " + " + str(self.target_dsp_freq) + " (" + str(self.actual_dsp_freq) + ")"
    def duck_copy(self, src):
        try:
            elems = filter(lambda x: (x[0] != "_") and hasattr(self, x), dir(src))
            map(lambda x: setattr(self, x, getattr(src, x)), elems)
            #for v in elems:
            #    print v
            #    setattr(self, v, getattr(src, v))
            return len(elems) > 0
        except Exception, e:
            print "~~> Failed to duck copy tune result:", src
            traceback.print_exc()

class GainRange():
    def __init__(self, start=0.0, stop=1.0, step=1.0):
        self.start = start
        self.stop = stop
        self.step = step

class Device():
    def __init__(self):
        self._running = False
        self._frequency = 0.0
        self._frequency_requested = 0.0
        self._gain = 0.0
        self._sample_rate = 0.0
        self._antenna = None
        self._last_error = None
    def is_running(self):
        return self._running
    def last_error(self):
        return self._last_error
    def start(self):
        if self.is_running():
            return True
        self._running = True
        return True
    def stop(self):
        if self.is_running() == False:
            return True
        self._running = False
        return True
    def open(self):
        self._antenna = self.antennas()[0]
        return True
    def close(self):
        if self.is_running():
            self.stop()
    def name(self):
        return "(no name)"
    def serial(self):
        return self.name()
    def gain_range(self):
        return GainRange()
    def master_clock(self):
        return 0
    def samples_per_packet(self):
        return 1024
    def antennas(self):
        return ["(Default)"]
    def gain(self, gain=None):
        if gain is not None:
            self._gain = gain
            return True
        return self._gain
    def sample_rate(self, rate=None):
        if rate is not None:
            if rate <= 0:
                return False
            self._sample_rate = rate
            return True
        return self._sample_rate
    def freq(self, freq=None, requested=None):
        if freq is not None:
            if freq > 0:
                if requested is not None and requested > 0:
                    self._frequency_requested = requested
                else:
                    self._frequency_requested = freq
                self._frequency = freq
                return True
            else:
                return False
        return self._frequency
    def was_tune_successful(self):
        return 0
    def last_tune_result(self):
        return TuneResult(self._frequency_requested, self.freq())
    def antenna(self, antenna=None):
        if antenna is not None:
            if type(antenna) == str:
                if len(antenna) == 0:
                    return False
                self._antenna = antenna
            elif type(antenna) == int:
                num = len(self.antennas())
                if antenna < 0 or antenna >= num:
                    return False
                self._antenna = self.antennas()[antenna]
            else:
                return False
            return True
        return self._antenna
    #def set_antenna(self, antenna):
    #    return True

class NetworkTransport():
    def __init__(self, default_port=28888):
        self._header = True
        self._default_port = default_port
        self._destination = ("127.0.0.1", default_port)
        self._payload_size = 0
    def destination(self, dest=None):
        if dest:
            if type(dest) == str:
                idx = dest.find(":")
                if idx > -1:
                    try:
                        port = int(dest[idx+1:])
                    except:
                        return False
                    dest = dest[0:idx].strip()
                    if len(dest) == 0 or port <= 0:
                        return False
                    self._destination = (dest, port)
                else:
                    self._destination = (dest, self._default_port)
            elif type(dest) == tuple:
                self._destination = dest
            else:
                return False
            return True
        return self._destination
    def header(self, enable=None):
        if enable is not None:
            self._header = enable
        return self._header
    def payload_size(self,size=None):
        if size is not None:
            self._payload_size = size
        return self._payload_size

def _delete_blocks(tb, done=[], verbose=False):
    tb.disconnect_all()
    for i in dir(tb):
        #if i == "_tb": # Must delete this too!
        #    continue
        obj = getattr(tb, i)
        if str(obj) in done:
            continue
        delete = False
        if issubclass(obj.__class__, gr.hier_block2):
            if verbose:
                print ">>> Descending:", i
            _delete_blocks(obj, done + [str(obj)])   # Prevent self-referential loops
            delete = True
        if delete or '__swig_destroy__' in dir(obj):
            done += [str(obj)]
            if verbose:
                print ">>> Deleting:", i
            exec "del tb." + i
    #while len(done) > 0:    # Necessary
    #    del done[0]

class GnuRadioDevice(Device, NetworkTransport):
    def __init__(self, flowgraph, options, sink=None, fixed_flowgraph=False):
        Device.__init__(self)
        NetworkTransport.__init__(self, options.port)
        self.flowgraph = flowgraph
        self.options = options
        self.sink = sink
        self.no_delete = fixed_flowgraph
        self.no_stop = fixed_flowgraph
        if fixed_flowgraph:
            self._running = True
        self._last_tune_result = None
    def open(self): # Can raise exceptions
        #try:
        if self.sink is None:
            if hasattr(self.flowgraph, 'sink') == False:
                print "~~> Failed to find 'sink' block in flowgraph"
                return False
            self.sink = self.flowgraph.sink
        payload_size = self.samples_per_packet() * 2 * 2    # short I/Q
        max_payload_size = (65536 - 29) # Max UDP payload
        max_payload_size -= (max_payload_size % 512)
        if payload_size > max_payload_size:
            print "!!> Restricting calculated payload size:", payload_size, "to maximum:", max_payload_size
            payload_size = max_payload_size
        self.payload_size(payload_size)
        return Device.open(self)
        #except Exception, e:
        #    print "Exception while initialising GNU Radio wrapper:", str(e)
        #return False
    def close(self):
        Device.close(self)
        if self.no_delete == False:
            _delete_blocks(self.flowgraph, verbose=self.options.debug)
    # Helpers
    def _get_helper(self, names, fallback=None, _targets=[]):
        if isinstance(names, str):
            names = [names]
        targets = _targets + ["", ".source", ".flowgraph"]  # Second 'flowgraph' for 'ServerWrapper'
        target = None
        name = None
        for n in names:
            for t in targets:
                #parts = t.split('.')
                #accum = []
                #next = False
                #for part in parts:
                #    if hasattr(eval("self.flowgraph" + ".".join(accum)), part) == False:
                #        next = True
                #        break
                #    accum += [part]
                #if next:
                #    continue
                try:
                    if hasattr(eval("self.flowgraph" + t), n) == False:
                        #print "    Not found:", t, "in:", n
                        continue
                except AttributeError:
                    #print "    AttributeError:", t, "in:", n
                    continue
                #print "    Found:", t, "in:", n
                target = t
                name = n
                break
            if target is not None:
                #print "    Using:", t, "in:", n
                break
        if target is None:
            if self.options.debug:
                print "##> Helper fallback:", names, fallback
            return fallback
        helper_name = "self.flowgraph" + target + "." + name
        helper = eval(helper_name)
        if self.options.debug:
            print "##> Helper found:", helper_name, helper
        if hasattr(helper, '__call__'):
            return helper
        return lambda: helper
    # Device
    def start(self):
        if self.is_running():
            return True
        try:
            self.flowgraph.start()
        except Exception, e:
            self._last_error = str(e)
            return False
        return Device.start(self)
    def stop(self):
        if self.is_running() == False:
            return True
        if self.no_stop == False:
            try:
                self.flowgraph.stop()
                self.flowgraph.wait()
            except Exception, e:
                self._last_error = str(e)
                return False
        return Device.stop(self)
    def name(self): # Raises
        try:
            fn = self._get_helper('source_name')
            if fn:
                return fn()
            return self._get_helper('name', self.flowgraph.name, [".source"])()
        except Exception, e:
            self._last_error = str(e)
            raise Exception, e
    def serial(self): # Raises
        try:
            return self._get_helper(['serial', 'serial_number'], lambda: Device.serial(self))()
        except Exception, e:
            self._last_error = str(e)
            raise Exception, e
    def gain_range(self): # Raises
        try:
            fn = self._get_helper(['gain_range', 'get_gain_range'])
            if fn is None:
                return Device.gain_range(self)
            _gr = fn()
            if isinstance(_gr, GainRange):
                return _gr
            try:
                gr = GainRange(_gr[0], _gr[1])
                if len(_gr) > 2:
                    gr.step = _gr[2]
                return gr
            except:
                pass
            try:
                gr = GainRange(_gr.start(), _gr.stop())
                if hasattr(_gr, 'step'):
                    gr.step = _gr.step()
                return gr
            except:
                pass
            raise Exception, "Unknown type returned from gain_range"
        except Exception, e:
            self._last_error = str(e)
            #return Device.gain_range(self)
            raise Exception, e
    def master_clock(self): # Raises
        return self._get_helper(['master_clock', 'get_clock_rate'], lambda: Device.master_clock(self))()
    def samples_per_packet(self):
        return self._get_helper(['samples_per_packet', 'recv_samples_per_packet'], lambda: Device.samples_per_packet(self))()
    def antennas(self):
        return self._get_helper(['antennas', 'get_antennas'], lambda: Device.antennas(self))()
    def gain(self, gain=None):
        if gain is not None:
            fn = self._get_helper('set_gain')
            if fn:
                res = fn(gain)
                if res is not None and res == False:
                    return False
            return Device.gain(self, gain)
        return self._get_helper(['gain', 'get_gain'], lambda: Device.gain(self))()
    def sample_rate(self, rate=None):
        if rate is not None:
            fn = self._get_helper(['set_sample_rate', 'set_samp_rate'])
            if fn:
                res = fn(rate)
                if res is not None and res == False:
                    return False
            return Device.sample_rate(self, rate)
        return self._get_helper(['sample_rate', 'samp_rate', 'get_sample_rate', 'get_samp_rate'], lambda: Device.sample_rate(self))()
    def freq(self, freq=None):
        if freq is not None:
            fn = self._get_helper(['set_freq', 'set_frequency', 'set_center_freq'])
            res = None
            if fn:
                res = fn(freq)
                if res is not None and res == False:
                    return False
                self._last_tune_result = None
            #if type(res) is int or type(res) is float:
            tuned = freq
            if res is not None and type(res) is not bool:
                try:
                    if self.options.debug:
                        print "##> Frequency set returned:", res
                    if type(res) is list or type(res) is tuple:
                        self._last_tune_result = TuneResult(freq, tuned)   # Should be same as res[0]
                        try:
                            #self._last_tune_result.target_rf_freq = res[0]
                            self._last_tune_result.actual_rf_freq = res[1]
                            self._last_tune_result.target_dsp_freq = res[2]
                            self._last_tune_result.actual_dsp_freq = res[3]
                            if self.options.debug:
                                print "##> Stored tune result:", self._last_tune_result
                        except Exception, e:
                            if self.options.debug:
                                print "##> Error while storing tune result:", e
                        tuned = self._last_tune_result.actual_rf_freq + self._last_tune_result.actual_dsp_freq
                    else:
                        temp_tune_result = TuneResult(freq, tuned)
                        if temp_tune_result.duck_copy(res):
                            if self.options.debug:
                                print "##> Duck copied tune result"
                            self._last_tune_result = temp_tune_result
                        else:
                            if self.options.debug:
                                print "##> Casting tune result to float"
                            tuned = float(res)
                            self._last_tune_result = None   # Will be created in call to Device
                except Exception, e:
                    if self.options.verbose:
                        print "##> Unknown exception while using response from frequency set:", str(res), "-", str(e)
            return Device.freq(self, tuned, freq)
        return self._get_helper(['freq', 'frequency', 'get_center_freq'], lambda: Device.freq(self))()
    def was_tune_successful(self):
        fn = self._get_helper(['was_tune_successful', 'was_tuning_successful'])
        if fn:
            return fn()
        tolerance = None
        fn = self._get_helper(['tune_tolerance', 'tuning_tolerance'])
        if fn:
            tolerance = fn()
        if tolerance is not None:
            tr = self.last_tune_result()
            diff = self.freq() - (tr.actual_rf_freq + tr.actual_dsp_freq)   # self.actual_dsp_freq
            if abs(diff) > tolerance:
                print "    Difference", diff, ">", tolerance, "for", tr
                if diff > 0:
                    return 1
                else:
                    return -1
        return Device.was_tune_successful(self)
    def last_tune_result(self):
        fn = self._get_helper(['last_tune_result'])
        if fn:
            return fn()
        if self._last_tune_result is not None:
            return self._last_tune_result
        return Device.last_tune_result(self)
    def antenna(self, antenna=None):
        if antenna is not None:
            fn = self._get_helper('set_antenna')
            if fn:
                if type(antenna) == int:
                    num = len(self.antennas())
                    if antenna < 0 or antenna >= num:
                        return False
                    antenna = self.antennas()[antenna]
                if len(antenna) == 0:
                    return False
                try:
                    res = fn(antenna)
                    if res is not None and res == False:
                        return False
                except:
                    return False
            return Device.antenna(self, antenna)
        return self._get_helper(['antenna', 'get_antenna'], lambda: Device.antenna(self))()
    # Network Transport
    def destination(self, dest=None):
        if dest is not None:
            prev = self._destination
            if NetworkTransport.destination(self, dest) == False:
                return False
            try:
                #print "--> Connecting UDP Sink:", self._destination[0], self._destination[1]
                self.sink.connect(self._destination[0], self._destination[1])
            except Exception, e:
                NetworkTransport.destination(self, prev)
                self._last_error = str(e)
                return False
            return True
        return NetworkTransport.destination(self)
    def header(self, enable=None):
        if enable is not None:
            self.sink.set_borip(enable)
        return NetworkTransport.header(self, enable)
    def payload_size(self,size=None):
        if size is not None:
            self.sink.set_payload_size(size)
        return NetworkTransport.payload_size(self, size)

def _format_error(error, pad=True):
    if error is None or len(error) == 0:
        return ""
    error = error.strip()
    error.replace("\\", "\\\\")
    error.replace("\r", "\\r")
    error.replace("\n", "\\n")
    if pad:
        error = " " + error
    return error

def _format_device(device, transport):
    if device is None or transport is None:
        return "-"
    return "%s|%f|%f|%f|%f|%d|%s|%s" % (
        device.name(),
        device.gain_range().start,
        device.gain_range().stop,
        device.gain_range().step,
        device.master_clock(),
        #device.samples_per_packet(),
        (transport.payload_size()/2/2),
        ",".join(device.antennas()),
        device.serial()
    )

def _create_device(hint, options):
    if options.verbose:
        print "--> Creating device with hint:", hint
    id = None
    if (options.default is not None) and (hint is None or len(hint) == 0 or hint == "-"):
        id = options.default
    if (id is None) or (len(id) == 0):
        id = _device_hint_mapper(hint)
    if options.debug:
        print "--> ID:", id
    if id is None or len(id) == 0:
        #return None
        raise Exception, "Empty ID"
    
    if isinstance(id, dict):
        args = []
        for arg in id['args']:
            check = _remove_quoted(arg)
            if check.count("(") != check.count(")"):
                continue
            args += [arg]
        id = id['module']   # Must come last
    else:
        parts = hint.split(" ")
        if len(parts) > 0 and parts[0].lower() == id.lower():
            parts = parts[1:]
        if len(parts) > 0:
            if options.debug:
                print "--> Hint parts:", parts
        args = []
        append = False
        accum = ""
        for part in parts:
            quote_list = _find_quotes(part)
            
            if (len(quote_list) % 2) != 0:  # Doesn't handle "a""b" as separate args
                if append == False:
                    append = True
                    accum = part
                    continue
                else:
                    part = accum + part
                    accum = ""
                    append = False
                    quote_list = _find_quotes(part)
            elif append == True:
                accum += part
                continue
            
            quotes = True
            key = None
            value = part
            
            idx = part.find("=")
            if idx > -1 and (len(quote_list) == 0 or idx < quote_list[0]):
                key = part[0:idx]
                value = part[idx + 1:]
            
            if len(quote_list) >= 2 and quote_list[0] == 0 and quote_list[-1] == (len(part) - 1):
                quotes = False
            else:
                if quotes:
                    try:
                        dummy = float(value)
                        quotes = False
                    except:
                        pass
                
                if quotes:
                    try:
                        dummy = int(value, 16)
                        quotes = False
                        dummy = value.lower()
                        if len(dummy) < 2 or dummy[0:2] != "0x":
                            value = "0x" + value
                    except:
                        pass
            
            arg = ""
            if key:
                arg = key + "="
            
            if quotes:
                value = value.replace("\"", "\\\"")
                arg += "\"" + value + "\""
            else:
                arg += value
            
            check = _remove_quoted(arg)
            if check.count("(") != check.count(")"):
                continue
            
            args += [arg]
    
    args_str = ",".join(args)
    if len(args_str) > 0:
        if options.debug:
            print "--> Args:", args_str
    
    if sys.modules.has_key("borip_" + id):
        if options.no_reload == False:
            try:
                #exec "reload(borip_" + id + ")"
                module = sys.modules["borip_" + id]
                print "--> Reloading:", module
                reload(module)
            except Exception, e:
                print "~~> Failed to reload:", str(e)
                #return None
                raise Exception, e
        
    #    try:
    #        device = module["borip_" + id + "(" + args_str + ")")
    #    except Exception, e:
    #        print "~~> Failed to create from module:", str(e)
    #        #return None
    #        raise Exception, e
    #else:
    try:
        exec "import borip_" + id
    except Exception, e:
        print "~~> Failed to import:", str(e)
        traceback.print_exc()
        #return None
        raise Exception, e
    
    try:
        device = eval("borip_" + id + ".borip_" + id + "(" + args_str + ")")
    except Exception, e:
        print "~~> Failed to create:", str(e)
        traceback.print_exc()
        #return None
        raise Exception, e
    
    device = _wrap_device(device, options)
    print "--> Created device:", device
    return device

def _wrap_device(device, options):
    #parents = device.__class__.__bases__
    #if Device not in parents:
    if issubclass(device.__class__, Device) == False:
        try:
            if isinstance(device, server) and device.parent is not None:
                print "--> Using GnuRadioDevice wrapper for parent", device.parent, "of", device
                device = GnuRadioDevice(device.parent, options, device.sink, options.fixed_flowgraph)
            #if gr.top_block in parents:
            elif issubclass(device.__class__, gr.top_block):
                print "--> Using GnuRadioDevice wrapper for", device
                device = GnuRadioDevice(device, options)
            elif issubclass(device.__class__, gr.hier_block2):
                print "--> Using Server and GnuRadioDevice wrapper for", device
                device = GnuRadioDevice(ServerWrapper(device, options), options)
            else:
                print "~~> Device interface in", device, "not found:", parents
                #return None
                raise Exception, "Device interface not found"
        except Exception, e:
            print "~~> Failed to wrap:", str(e)
            traceback.print_exc()
            #return None
            raise Exception, e
    
    #parents = device.__class__.__bases__
    #if NetworkTransport not in parents:
    if issubclass(device.__class__, NetworkTransport) == False:
        print "~~> NetworkTransport interface in", device, "not found:", parents
        #return None
        raise Exception, "NetworkTransport interface not found"
    
    try:
        if device.open() == False:
            print "~~> Failed to initialise device:", device
            device.close()
            #return None
            raise Exception, "Failed to initialise device"
    except Exception, e:
        print "~~> Failed to open:", str(e)
        traceback.print_exc()
        #return None
        raise Exception, e
    
    return device

def _find_quotes(s):
    b = False
    e = False
    list = []
    i = -1
    for c in s:
        i += 1
        if c == '\\':
            e = True
            continue
        elif e:
            e = False
            continue
        
        if c == '"':
            list += [i]
    return list

def _remove_quoted(data):
    r = ""
    list = _find_quotes(data)
    if len(list) == 0:
        return data
    last = 0
    b = False
    for l in list:
        if b == False:
            r += data[last:l]
            b = True
        else:
            last = l + 1
            b = False
    if b == False:
        r += data[last:]
    return r

class ThreadedTCPRequestHandler(SocketServer.StreamRequestHandler): # BaseRequestHandler
    # No __init__
    def setup(self):
        SocketServer.StreamRequestHandler.setup(self)
        
        print "==> Connection from:", self.client_address
        
        self.device = None
        self.transport = NetworkTransport() # Create dummy to avoid 'None' checks in code
        
        device = None
        if self.server.block:
            if self.server.device:
                self.server.device.close()
                self.server.device = None
            device = _wrap_device(self.server.block, self.server.options)
        elif self.server.device:
            device = self.server.device
        if device:
            transport = device # MAGIC
            if transport.destination(self.client_address[0]):
                self.device = device
                self.transport = transport
            else:
                device.close()
        
        with self.server.clients_lock:
            self.server.clients.append(self)
        
        first_response = "DEVICE " + _format_device(self.device, self.transport)
        if self.server.options.command:
            print "< " + first_response
        self.send(first_response)
    def handle(self):
        buffer = ""
        while True:
            data = ""   # Initialise to nothing so if there's an exception it'll disconnect
            
            try:
                data = self.request.recv(1024)  # 4096
            except socket.error, (e, msg):
                if e != 104:    # Connection reset by peer
                    print "==>", self.client_address, "-", msg
            
            #data = self.rfile.readline().strip()
            if len(data) == 0:
                break
            
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
                if self.process_client_command(line) == False:
                    break
            else:
                buffer = ""
    def finish(self):
        print "==> Disconnection from:", self.client_address
        
        if self.device:
            print "--> Closing device:", self.device
        self.close_device()
        
        with self.server.clients_lock:
            self.server.clients.remove(self)
        
        try:
            SocketServer.StreamRequestHandler.finish(self)
        except socket.error, (e, msg):
            if e != 32:    # Broken pipe
                print "==>", self.client_address, "-", msg
    def close_device(self):
        if self.device:
            self.device.close()
            #del self.device
            #gc.collect()
            self.device = None
        self.transport = NetworkTransport() # MAGIC
    def process_client_command(self, command):
        if self.server.options.command:
            print ">", command
        data = None
        idx = command.find(" ")
        if idx > -1:
            data = command[idx+1:].strip();
            command = command[0:idx]
        command = command.upper()
        result = "OK"
        
        try:
            if command == "GO":
                if self.device:
                    if self.device.is_running():
                        result += " RUNNING"
                    else:
                        if self.device.start() != True:
                            result = "FAIL" + _format_error(self.device.last_error())
                else:
                    result = "DEVICE"
            
            elif command == "STOP":
                if self.device:
                    if self.device.is_running():
                        result += " STOPPED"
                    self.device.stop()
                else:
                    result = "DEVICE"
            
            elif command == "DEVICE":
                error = ""
                if (self.server.options.lock == False) and data is not None and (len(data) > 0):
                    self.close_device()
                    
                    if data != "!":
                        try:
                            device = _create_device(data, self.server.options)
                            if device:
                                transport = device    # MAGIC
                                if transport.destination(self.client_address[0]) == False:
                                    error = "Failed to initialise NetworkTransport"
                                    device.close()
                            else:
                                error = "Failed to create device"
                        except Exception, e:
                            traceback.print_exc()
                            error = str(e)
                        if len(error) == 0:
                            self.device = device
                            self.transport = transport
                result = _format_device(self.device, self.transport) + _format_error(error)
            
            elif command == "FREQ":
                if self.device:
                    if data is None:
                        result = str(self.device.freq())
                    else:
                        freq = 0.0
                        try:
                            freq = float(data)
                        except:
                            pass
                        
                        if self.device.freq(freq):
                            success = self.device.was_tune_successful()
                            if success < 0:
                                result = "LOW"
                            elif success > 0:
                                result = "HIGH"
                            
                            tune_result = self.device.last_tune_result()
                            result += " %f %f %f %f" % (tune_result.target_rf_freq, tune_result.actual_rf_freq, tune_result.target_dsp_freq, tune_result.actual_dsp_freq)
                else:
                    result = "DEVICE"
            
            elif command == "ANTENNA":
                if self.device:
                    if data is None:
                        result = str(self.device.antenna())
                        if result is None or len(result) == 0:
                            result = "UNKNOWN"
                    else:
                        if self.device.antenna(data) == False:
                            result = "FAIL" + _format_error(self.device.last_error())
                else:
                    result = "DEVICE"
            
            elif command == "GAIN":
                if self.device:
                    if data is None:
                        result = str(self.device.gain())
                    else:
                        gain = 0.0
                        try:
                            gain = float(data)
                        except:
                            pass
                        
                        if self.device.gain(gain):
                            #result += " " + str(self.device.gain())
                            pass
                        else:
                            result = "FAIL" + _format_error(self.device.last_error())
                else:
                    result = "DEVICE"
            
            elif command == "RATE":
                if self.device:
                    if data is None:
                        result = str(self.device.sample_rate())
                    else:
                        rate = 0.0
                        try:
                            rate = float(data)
                        except:
                            pass
                        
                        if self.device.sample_rate(rate):
                            result += " " + str(self.device.sample_rate())
                        else:
                            result = "FAIL" + _format_error(self.device.last_error())
                else:
                    result = "DEVICE"
            
            elif command == "DEST":
                if data is None:
                    result = self.transport.destination()[0] + ":" + str(self.transport.destination()[1])
                else:
                    if data == "-":
                        data = self.client_address[0]
                    
                    if self.transport.destination(data):
                        result += " " + self.transport.destination()[0] + ":" + str(self.transport.destination()[1])
                    else:
                        result = "FAIL Failed to set destination"
            
            elif command == "HEADER":
                if data is None:
                    if self.transport.header():
                        result = "ON"
                    else:
                        result = "OFF"
                else:
                    self.transport.header(data.upper() == "OFF")
            
            #########################################
            
            #elif command == "SHUTDOWN":
            #    quit_event.set()
            #    return False
            
            #########################################
            
            else:
                result = "UNKNOWN"
        except Exception, e:
            if command == "DEVICE":
                result = "-"
            else:
                result = "FAIL"
            result += " " + str(e)
            traceback.print_exc()
        
        if result is None or len(result) == 0:
            return True
        
        result = command + " " + result
        
        if self.server.options.command:
            print "<", result
        
        return self.send(result)
    def send(self, data):
        try:
            self.wfile.write(data + "\n")
            return True
        except socket.error, (e, msg):
            if e != 32:    # Broken pipe
                print "==>", self.client_address, "-", msg
        return False

class ThreadedTCPServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
    allow_reuse_address = True

def _generate_options(args=None):
    usage="%prog: [options]"
    parser = OptionParser(usage=usage)  #option_class=eng_option, 
    parser.add_option("-l", "--listen", type="int", help="server listen port", default=28888)  #, metavar="SUBDEV"
    parser.add_option("-p", "--port", type="int", help="default data port", default=28888)
    parser.add_option("-v", "--verbose", action="store_true", help="verbose output", default=False)
    parser.add_option("-g", "--debug", action="store_true", help="debug output", default=False)
    parser.add_option("-c", "--command", action="store_true", help="command output", default=False)
    parser.add_option("-r", "--realtime", action="store_true", help="realtime scheduling", default=False)
    parser.add_option("-R", "--no-reload", action="store_true", help="disable dynamic reload", default=False)
    parser.add_option("-d", "--device", type="string", help="immediately create device", default=None)
    parser.add_option("-L", "--lock", action="store_true", help="lock device", default=False)
    parser.add_option("-D", "--default", type="string", help="device to create when default hint supplied", default=None)
    
    if args:
        if not isinstance(args, list):
            args = [args]
        (options, args) = parser.parse_args(args)
    else:
        (options, args) = parser.parse_args()
    
    options.fixed_flowgraph = False
    
    return (options, args)

class borip_server():
    def __init__(self, block=None, options=None):
        self.server = None
        self.server_thread = None
        
        if options is None or isinstance(options, str):
            (options, args) = _generate_options(options)
        self.options = options
        
        self.block = block
    def __del__(self):
        self.stop()
    def start(self):
        if self.server is not None:
            return True
        
        device = None
        if self.block is None and self.options.device is not None:
            try:
                device = _create_device(self.options.device, self.options)
            except Exception, e:
                print "!!> Failed to create initial device with hint:", self.options.device
        
        HOST, PORT = "", self.options.listen
        print "==> Starting TCP server on port:", PORT
        while True:
            try:
                self.server = ThreadedTCPServer((HOST, PORT), ThreadedTCPRequestHandler)
                self.server.options = self.options
                self.server.block = self.block
                self.server.device = device
                self.server.clients_lock = threading.Lock()
                self.server.clients = []
                #ip, port = self.server.server_address
                self.server_thread = threading.Thread(target=self.server.serve_forever)
                self.server_thread.setDaemon(True)
                self.server_thread.start()
                print "==> TCP server running on", self.server.server_address, "in thread:", self.server_thread.getName()
            except socket.error, (e, msg):
                print "    Socket error:", msg
                if (e == 98):   # Still in use
                    print "    Waiting, then trying again..."
                    try:
                        time.sleep(5)
                    except KeyboardInterrupt:
                        print "    Aborting"
                        sys.exit(1)
                    continue
                sys.exit(1)
            break
        
        if self.options.realtime:
            if gr.enable_realtime_scheduling() == gr.RT_OK:
                print "==> Enabled realtime scheduling"
            else:
                print "!!> Failed to enable realtime scheduling"
        return True
    def stop(self):
        if self.server is None:
            return True
        print "==> Closing server..."
        self.server.server_close()   # Clients still exist
        with self.server.clients_lock:
            # Calling 'shutdown' here causes clients not to handle close via 'finish'
            for client in self.server.clients:
                print "<<< Closing:", client.client_address
                try:
                    client.request.shutdown(socket.SHUT_RDWR)
                    client.request.close()
                except:
                    pass
            self.server.shutdown()
            self.server_thread.join()
        self.server = None
        return True

def main():
    server = borip_server()
    server.start()
    
    try:
        while True:
            raw_input()
    except KeyboardInterrupt:
        print
    except EOFError:
        pass
    except Exception, e:
        print "==> Unhandled exception:", str(e)
    
    server.stop()
    print "==> Done"

if __name__ == "__main__":
    main()
