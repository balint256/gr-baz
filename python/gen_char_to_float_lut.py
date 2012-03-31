#!/usr/bin/env python

import sys

sys.stdout.write("{ ");
for i in range(256):
	f = (float(i) - 127.5) / 127.5
	sys.stdout.write("%ff" % (f))
	if i < 255:
		sys.stdout.write(", ")
print(" };")
