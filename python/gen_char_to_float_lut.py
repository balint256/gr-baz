#!/usr/bin/env python

import sys

sys.stdout.write("{ ");
for i in range(256):
	f = (float(i) - 128.0) / 128.0
	sys.stdout.write("%ff" % (f))
	if i < 255:
		sys.stdout.write(", ")
print(" };")
