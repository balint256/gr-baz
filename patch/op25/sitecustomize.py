import os, sys

_sc_path = os.path.abspath( __file__ )
try:
	print "Customising for:", _sc_path.split(os.sep)[-5]
except:
	print "[!] Customising for:", _sc_path

_gr_base_path = None
try:
	_gr_base_path = os.path.dirname(_sc_path)
	sys.path = [os.path.join(_gr_base_path, 'gnuradio')] + sys.path
except:
	print "Failed to add gnuradio in sitecustomize:", _gr_base_path

import op25		# From local sitecustomize'd gnuradio directory
import gnuradio		# From global installation
gnuradio.op25 = op25	# Inject local module into global
