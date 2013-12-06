#!/usr/bin/env python
##################################################
# Gnuradio Python Flow Graph
# Title: Borip Usrp Uhd
# Generated: Tue Nov 20 23:43:41 2012
##################################################

from gnuradio import eng_notation
from gnuradio import gr
from gnuradio import uhd
from gnuradio.eng_option import eng_option
from gnuradio import filter 
from gnuradio.filter import firdes
from optparse import OptionParser
import baz

class borip_usrp_uhd(gr.top_block):

	def __init__(self, addr="", subdev=""):
		gr.top_block.__init__(self, "Borip Usrp Uhd")

		##################################################
		# Parameters
		##################################################
		self.addr = addr
		self.subdev = subdev

		##################################################
		# Variables
		##################################################
		self.source_name = source_name = lambda: "USRP (" + self.source.get_usrp_info().get("mboard_id") + ")"
		self.serial = serial = lambda: self.source.get_usrp_info().get("mboard_serial")

		##################################################
		# Blocks
		##################################################
		self.source = uhd.usrp_source(
			device_addr=addr,
			stream_args=uhd.stream_args(
				cpu_format="sc16",
				channels=range(1),
			),
		)
		#self.source.set_samp_rate(0)
		#self.source.set_center_freq(0, 0)
		#self.source.set_gain(0, 0)
		self.sink = baz.udp_sink(gr.sizeof_short*2, "", 28888, 1472, False, True)

		##################################################
		# Connections
		##################################################
		self.connect((self.source, 0), (self.sink, 0))

	def get_addr(self):
		return self.addr

	def set_addr(self, addr):
		self.addr = addr

	def get_subdev(self):
		return self.subdev

	def set_subdev(self, subdev):
		self.subdev = subdev

	def get_source_name(self):
		return self.source_name

	def set_source_name(self, source_name):
		self.source_name = source_name

	def get_serial(self):
		return self.serial

	def set_serial(self, serial):
		self.serial = serial

if __name__ == '__main__':
	parser = OptionParser(option_class=eng_option, usage="%prog: [options]")
	parser.add_option("-a", "--addr", dest="addr", type="string", default="",
		help="Set Address [default=%default]")
	parser.add_option("-s", "--subdev", dest="subdev", type="string", default="",
		help="Set Sub Dev [default=%default]")
	(options, args) = parser.parse_args()
	tb = borip_usrp_uhd(addr=options.addr, subdev=options.subdev)
	tb.start()
	raw_input('Press Enter to quit: ')
	tb.stop()

