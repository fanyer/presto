#!/bin/env python

import platform as py_platform
import sys, os
from optparse import OptionParser


supported_platforms = ['unix', 'windows', 'mac']

def __get_platform():
	os = py_platform.system().lower()
	if os in ['darwin', 'mac']:
		os = 'mac'
	elif os in ['linux', 'bsd', 'unix']:
		os = 'unix'
	elif os in ['windows', 'cygwin_nt-6.1-wow64', 'cygwin_nt-5.1']:
		os = 'windows'
	assert( os in supported_platforms ), "%s is not valid platform!" % os
	return os

platform = __get_platform()


def __run_unix_selftests(test=''):
	assert( platform == 'unix' )
	cmdline = "run/opera -pd profile_selftest %s" % test
	if __options.verbose:
		print ":: run %s" % cmdline
	os.system(cmdline)


def __run_windows_selftests(test=''):
	assert( platform == 'windows' )
	# cygwin expects '/' after all
	cmdline = os.path.join('Debug', 'opera.exe %s' % test)
	if __options.verbose:
		print ":: run %s" % cmdline
	os.system(cmdline)


def __run_mac_selftests(test=''):
	assert( platform == 'mac' )
	cmdline = "output/Debug/Opera.app/Contents/MacOS/Opera %s" % test
	if __options.verbose:
		print ":: run %s" % cmdline
	os.system(cmdline)

def run_selftests(tests = []):
	__selftest_functions = { 
	'unix'		: __run_unix_selftests,
	'windows'	: __run_windows_selftests,
	'mac'		: __run_mac_selftests,
	}
	args = [ '-test=%s' % x for x in tests ]
	args_str = ' '.join(args)
	if( __options.output_file_path != None ):
		args_str = args_str + " -test-output=" + __options.output_file_path
	run_fun = __selftest_functions[platform]
	assert( callable(run_fun) )
	if( args_str ):
		run_fun( args_str )


def groupnames_iterator(filename):
	for name in open(filename):
		name=name.strip()
		if not name or name.startswith('#'):
			continue
		xplatform = True
		for prefix in supported_platforms:
			if name.startswith(prefix):
				xplatform = False
				if prefix == platform: yield name
		if xplatform:
			yield name


def find_module(name):
	if name.endswith('module.selftests'):
		return name
	for prefix in ['adjunct', 'modules', 'platforms']:
		if name.startswith(prefix): 
			name = os.path.join(name, 'module.selftests')
			return name
	for prefix in ['adjunct', 'modules', 'platforms']:
		tname = os.path.join(prefix, name) 
		tname = os.path.join(tname, 'module.selftests')
		if os.path.exists(tname):
			return tname
	return name


def parse_commandline():
	usage = """usage: %prog [options] module

	This is convenience tool for running all selftests that are useful to
	particular module, even if they are outside of this module.
	Script assumes, that it will be run from root project directory."""
	parser = OptionParser(usage=usage)
	parser.add_option("-o", "--output",
                  dest="output_file_path", metavar="FILE",
                  help="output test results to FILE")
	parser.add_option("-q", "--quiet",
                  action="store_false", dest="verbose", default=True,
                  help="don't print status messages to stdout")
	global __options
	__options, args = parser.parse_args()
	if not args:
		parser.print_help()
		sys.exit(0)
	return args[0]


def main():
	arg_module = parse_commandline()
	test_list = find_module(arg_module)
	assert( test_list )
	if __options.verbose:
		print ":: run selftests from %s" % test_list
	try:
		test_groups = [ x for x in groupnames_iterator(test_list) ]
		run_selftests( test_groups )
	except IOError:
		print 'ops! %s cannot be opened' % test_list


if __name__=='__main__':
	main()
