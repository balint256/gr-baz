#!/usr/bin/env python

import sys

sys.stdout.write("{ ");
for i in range(256):
	f = float(i) - 128
	sys.stdout.write("%.1ff" % (f))
	if i < 255:
		sys.stdout.write(", ")
print(" };")
