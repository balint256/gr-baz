#!/usr/bin/env python
##################################################
# Gnuradio Python Flow Graph
# Title: Borip Rtl2
# Generated: Mon May 28 17:08:30 2012
##################################################

from gnuradio import eng_notation
from gnuradio import gr
from gnuradio.eng_option import eng_option
from gnuradio import filter 
from gnuradio.filter import firdes
from optparse import OptionParser
import baz

class borip_RTL2(gr.top_block):

	def __init__(self, buf=True, tuner=""):
		gr.top_block.__init__(self, "Borip Rtl2")

		##################################################
		# Parameters
		##################################################
		self.buf = buf
		self.tuner = tuner

		##################################################
		# Variables
		##################################################
		self.master_clock = master_clock = 3200000

		##################################################
		# Message Queues
		##################################################
		source_msgq_out = sink_msgq_in = gr.msg_queue(2)

		##################################################
		# Blocks
		##################################################
		self.sink = gr.udp_sink(gr.sizeof_short*1, "127.0.0.1", 28888, 1472, False, True)
		self.sink.set_status_msgq(sink_msgq_in)
		self.source = baz.rtl_source_c(defer_creation=True, output_size=gr.sizeof_short)
		self.source.set_verbose(True)
		self.source.set_vid(0x0)
		self.source.set_pid(0x0)
		self.source.set_tuner_name(tuner)
		self.source.set_default_timeout(0)
		self.source.set_use_buffer(buf)
		self.source.set_fir_coefficients(([]))
		
		self.source.set_read_length(0)
		
		
		
		
		if self.source.create() == False: raise Exception("Failed to create RTL2832 Source: source")
		
		
		self.source.set_sample_rate(1000000)
		
		self.source.set_frequency(0)
		
		
		self.source.set_status_msgq(source_msgq_out)
		
		self.source.set_auto_gain_mode(False)
		self.source.set_relative_gain(False)
		self.source.set_gain(0)
		  

		##################################################
		# Connections
		##################################################
		self.connect((self.source, 0), (self.sink, 0))

	def set_buf(self, buf):
		self.buf = buf

	def set_tuner(self, tuner):
		self.tuner = tuner

	def set_master_clock(self, master_clock):
		self.master_clock = master_clock

if __name__ == '__main__':
	parser = OptionParser(option_class=eng_option, usage="%prog: [options]")
	parser.add_option("", "--tuner", dest="tuner", type="string", default="",
		help="Set Tuner [default=%default]")
	(options, args) = parser.parse_args()
	tb = borip_RTL2(tuner=options.tuner)
	tb.start()
	raw_input('Press Enter to quit: ')
	tb.stop()

