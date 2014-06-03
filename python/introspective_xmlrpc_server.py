#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  untitled.py
#  
#  Copyright 2014 Balint Seeber <balint@crawfish>
#  
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.
#  
#  

from SimpleXMLRPCServer import SimpleXMLRPCServer
#from SimpleXMLRPCServer import SimpleXMLRPCRequestHandler
import inspect

class IntrospectiveXMLRPCServer(SimpleXMLRPCServer):
	def __init__(self, address, signatures={}, *kargs, **kwargs):
		self.signatures = signatures
		SimpleXMLRPCServer.__init__(self, address, *kargs, **kwargs)
		#print "Init"
	def system_methodSignature(self, method_name):
		try:
			arg_list = []
			ret_val = ''
			if method_name in self.signatures:
				arg_list = self.signatures[method_name]
				if isinstance(arg_list, tuple):
					ret_val = arg_list[0]
					arg_list = arg_list[1]
			elif method_name in self.funcs:
				func = self.funcs[method_name]
				spec = inspect.getargspec(func)
				arg_list = spec.args
			elif self.instance is not None:
				#if len(method_name) > 4 and method_name[:4] == "get_":
				#	base_name
				#if hasattr(self.instance, "get_"+method_name+"_sig"):
				#	func = getattr(self.instance, "get_"+method_name+"_sig")
				#	arg_list = func()
				if hasattr(self.instance, method_name):
					func = getattr(self.instance, method_name)
					spec = inspect.getargspec(func)
					arg_list = spec.args[1:]	# Ignore 'self'
			#return str(inspect.signature(func))	# v3.4
			return [[ret_val] + arg_list]
		except KeyError, e:
			err_str = 'method "%s" can not be found' % method_name
			#raise Exception(err_str)
			print err_str
		except Exception, e:
			print "Exception:", e
		return [[]]

def main():
	return 0

if __name__ == '__main__':
	main()
