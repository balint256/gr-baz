import os

_sc_path = os.path.abspath( __file__ )
try:
	print "Customising for:", _sc_path.split(os.sep)[-5]
except:
	print "[!] Customising for:", _sc_path

import op25
import gnuradio
gnuradio.op25 = op25
