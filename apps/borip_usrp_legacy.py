#!/usr/bin/env python
##################################################
# Gnuradio Python Flow Graph
# Title: Borip Usrp Legacy
# Generated: Thu Nov 22 22:37:47 2012
##################################################

from gnuradio import eng_notation
from gnuradio import gr
from gnuradio.eng_option import eng_option
from gnuradio import filter 
from gnuradio.filter import firdes
from grc_gnuradio import usrp as grc_usrp
from optparse import OptionParser
import baz

class borip_usrp_legacy(gr.top_block):

	def __init__(self, unit=0, side="A"):
		gr.top_block.__init__(self, "Borip Usrp Legacy")

		##################################################
		# Parameters
		##################################################
		self.unit = unit
		self.side = side

		##################################################
		# Variables
		##################################################
		self.tr_to_list = tr_to_list = lambda req, tr: [req, tr.baseband_freq, tr.dxc_freq + tr.residual_freq, tr.dxc_freq]
		self.serial = serial = lambda: self.source._get_u().serial_number()
		self.master_clock = master_clock = lambda: self.source._get_u().fpga_master_clock_freq()
		self.tune_tolerance = tune_tolerance = 1
		self.source_name = source_name = lambda: "USRP (" + serial() + ")"
		self.set_samp_rate = set_samp_rate = lambda r: self.source.set_decim_rate(self.master_clock()//r)
		self.set_freq = set_freq = lambda f: self.tr_to_list(f, self.source._get_u().tune(0, self.source._subdev, f))
		self.set_antenna = set_antenna = lambda a: self.source._subdev.select_rx_antenna(a)
		self.samp_rate = samp_rate = lambda: self.master_clock()/self.source._get_u().decim_rate()
		self.gain_range = gain_range = lambda: self.source._subdev.gain_range()
		self.antennas = antennas = ["TX/RX","RX2","RXA","RXB","RXAB"]

		##################################################
		# Message Queues
		##################################################
		source_msgq_out = sink_msgq_in = gr.msg_queue(2)

		##################################################
		# Blocks
		##################################################
		self.source = grc_usrp.simple_source_s(which=unit, side=side, rx_ant="")
		self.source.set_decim_rate(256)
		self.source.set_frequency(0, verbose=True)
		self.source.set_gain(0)
		if hasattr(self.source, '_get_u') and hasattr(self.source._get_u(), 'set_status_msgq'): self.source._get_u().set_status_msgq(source_msgq_out)
		self.sink = baz.udp_sink(gr.sizeof_short*1, "", 28888, 1472, False, True)
		self.sink.set_status_msgq(sink_msgq_in)

		##################################################
		# Connections
		##################################################
		self.connect((self.source, 0), (self.sink, 0))

	def get_unit(self):
		return self.unit

	def set_unit(self, unit):
		self.unit = unit

	def get_side(self):
		return self.side

	def set_side(self, side):
		self.side = side

	def get_tr_to_list(self):
		return self.tr_to_list

	def set_tr_to_list(self, tr_to_list):
		self.tr_to_list = tr_to_list
		self.set_set_freq(lambda f: self.self.tr_to_list(f, self.source._get_u().tune(0, self.source._subdev, f)))

	def get_serial(self):
		return self.serial

	def set_serial(self, serial):
		self.serial = serial
		self.set_source_name(lambda: "USRP (" + self.serial() + ")")

	def get_master_clock(self):
		return self.master_clock

	def set_master_clock(self, master_clock):
		self.master_clock = master_clock
		self.set_set_samp_rate(lambda r: self.source.set_decim_rate(self.self.master_clock()//r))
		self.self.set_samp_rate(lambda: self.self.master_clock()/self.source._get_u().decim_rate())

	def get_tune_tolerance(self):
		return self.tune_tolerance

	def set_tune_tolerance(self, tune_tolerance):
		self.tune_tolerance = tune_tolerance

	def get_source_name(self):
		return self.source_name

	def set_source_name(self, source_name):
		self.source_name = source_name

	def get_set_samp_rate(self):
		return self.set_samp_rate

	def set_set_samp_rate(self, set_samp_rate):
		self.set_samp_rate = set_samp_rate
		self.self.set_samp_rate(lambda: self.self.master_clock()/self.source._get_u().decim_rate())

	def get_set_freq(self):
		return self.set_freq

	def set_set_freq(self, set_freq):
		self.set_freq = set_freq

	def get_set_antenna(self):
		return self.set_antenna

	def set_set_antenna(self, set_antenna):
		self.set_antenna = set_antenna

	def get_samp_rate(self):
		return self.samp_rate

	def set_samp_rate(self, samp_rate):
		self.samp_rate = samp_rate

	def get_gain_range(self):
		return self.gain_range

	def set_gain_range(self, gain_range):
		self.gain_range = gain_range
		self.set_gain_range(lambda: self.source._subdev.self.gain_range())

	def get_antennas(self):
		return self.antennas

	def set_antennas(self, antennas):
		self.antennas = antennas

if __name__ == '__main__':
	parser = OptionParser(option_class=eng_option, usage="%prog: [options]")
	parser.add_option("", "--unit", dest="unit", type="intx", default=0,
		help="Set Unit [default=%default]")
	parser.add_option("", "--side", dest="side", type="string", default="A",
		help="Set Side [default=%default]")
	(options, args) = parser.parse_args()
	tb = borip_usrp_legacy(unit=options.unit, side=options.side)
	tb.start()
	raw_input('Press Enter to quit: ')
	tb.stop()

