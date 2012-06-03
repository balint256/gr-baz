#!/usr/bin/env python

import sys

sys.stdout.write("{ ");
for i in range(256):
	f = (float(i) - 128.0) / 128.0
	d = int(f * 32768)
	sys.stdout.write("%d" % (d))
	if i < 255:
		sys.stdout.write(", ")
print(" };")
