#!/usr/bin/env python
#
# Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
#
# This file is part of the Opera web browser.
# It may not be distributed under any circumstances.

import sys
import os.path
import optparse
import time
import re
import math

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])), 'src'))

import dispatcher
import testmanager
import headless
import opera
import test
import log
import signal

def parsetest(testspec, deftimeout):
    if testspec.startswith('@'):
        res = []
        for line in open(testspec[1:]):
            res += parsetest(line, deftimeout)
        return res
    m = re.match(r'(?:([^!]+)!)?([-_A-Za-z0-9]+):(.*)', testspec)
    if m:
        name = m.group(1)
        testtype = m.group(2).lower()
        if testtype == 'spartan':
            m = re.match('(?:(\d+),)?(.*)', m.group(3))
            timeout = int(m.group(1)) if m.group(1) else deftimeout
            t = test.SPARTANTest(m.group(2), timeout)
        else:
            raise Exception("Unknown test type: %s" % m.group(2))
        if name:
            t.name = name
        return [t]
    else:
        raise Exception("Syntax error in test specification: %s" % testspec)

def report(tests, results, output):
    for i in xrange(len(tests)):
        if results[i] != None:
            print >> output, "%.3f, %s" % (results[i], tests[i].name)

op = optparse.OptionParser(
    usage="%prog [options] TEST...",
    version='autobench, consult git for version information',
    description='autobench runs benchmarks in Opera fully unattended'
    )
op.add_option('-O', '--opera',
              action='store',
              type='string',
              dest='opera',
              help='Path to Opera wrapper script to run.',
              metavar='DIR'
              )
op.add_option('-o', '--output',
              action='store',
              type='string',
              dest='output',
              help='Destination file for test results.',
              metavar='FILE'
              )
op.add_option('-l', '--logfile',
              action='store',
              type='string',
              dest='logfile',
              help='Write detailed log to the specified file.',
              metavar='FILE'
              )
op.add_option('-H', '--headless',
              action='store_true',
              dest='headlessmode',
              help='Run in headless mode (without a visible window). This is the default when DISPLAY is unset.'
              )
op.add_option('-g', '--geometry',
              action='store',
              type='string',
              dest='geometry',
              help='Size for Opera window (defaults to 1024x768).',
              metavar='WxH'
              )
op.add_option('-j', '--jobs',
              action='store',
              type='int',
              dest='jobs',
              help='Spawn N parallel jobs.',
              metavar='N'
              )
op.add_option('-r', '--repeat',
              action='store',
              type='int',
              dest='repeat',
              help='Repeat the test sequence N times (result will be averaged).',
              metavar='N'
              )
op.add_option('-t', '--timeout',
              action='store',
              type='int',
              dest='timeout',
              help='Default timeout for an individual test (defaults to 60 seconds), 0 for infinite.',
              metavar='SECONDS'
              )
op.add_option('-q', '--quiet',
              action='store_true',
              dest='quiet',
              help='Don\'t log to console.'
              )
op.set_defaults(opera='./opera', quiet=False, jobs=1, headlessmode=('DISPLAY' not in os.environ), repeat=1, timeout=60)
options, args = op.parse_args()

if len(args) < 1:
    op.error("Please specify at least one test to run")

tests = []
for testspec in args:
    tests += parsetest(testspec, options.timeout)

if options.geometry:
    m = re.match('(\d+)x(\d+)', options.geometry)
    if not m:
        raise Exception("Invalid setting for geometry: %s" % options.geometry)
    width = int(m.group(1))
    height = int(m.group(2))
else:
    width = 1024
    height = 768

if options.headlessmode:
    for cols in xrange(int(math.sqrt(options.jobs)), 0, -1):
        if options.jobs % cols == 0:
            rows = options.jobs / cols
            break
    vncserver = headless.VNCServer(width * cols, height * rows)
else:
    vncserver = None

def spawner(id, testManager, feedbackServer):
    return opera.OperaProcess(id, testManager, feedbackServer, options.opera, width, height, vncserver)

def sighandler(signum, frame):
    sys.exit(1)

signal.signal(signal.SIGTERM, sighandler)
signal.signal(signal.SIGHUP, sighandler)
signal.signal(signal.SIGINT, sighandler)

if options.quiet:
    log.logger.setquiet(True)
if options.logfile:
    log.logger.setlog(options.logfile)

testManager = testmanager.TestManager(tests, options.repeat, options.jobs)

try:
    if options.headlessmode:
        vncserver.start()
        time.sleep(1) # Let Xvnc settle
    testManager.start(spawner)
    exitcode = dispatcher.the.run()
    if options.output:
        report(tests, testManager.getResults(), open(options.output, 'w'))
    sys.exit(exitcode)
finally:
    testManager.cleanup()
    dispatcher.the.run()
    if vncserver:
        vncserver.cleanup()
