# Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
#
# This file is part of the Opera web browser.
# It may not be distributed under any circumstances.

import sys

import dispatcher

INFO, WARNING, ERROR, MAX = range(4)

def info(str, channel=None):
    logger.log(INFO, str, channel)

def warning(str, channel=None):
    logger.log(WARNING, str, channel)

def error(str, channel=None):
    logger.log(ERROR, str, channel)

class Logger(object):

    def __init__(self):
        self.stderr_min = INFO
        self.log_min = INFO
        self.logfile = None
        self.lasttime = None
        self.lastchannel = -1

    def setlog(self, filename):
        try:
            self.logfile = open(filename, 'w')
            self.log(INFO, "Logging to file %s" % filename, None)
        except IOError, e:
            self.log(ERROR, e, None)

    def setquiet(self, quiet):
        if quiet:
            self.stderr_min = ERROR
        else:
            self.stderr_min = INFO

    def log(self, severity, str, channel):

        if severity == INFO:
            s = '   '
        elif severity == WARNING:
            s = '[!]'
        elif severity == ERROR:
            s = '[X]'

        time = dispatcher.the.getTime()

        if channel == self.lastchannel and time == self.lasttime:
            c = ' ' * 3
        else:
            if channel == None:
                c = '---'
            else:
                c = "<%c>" % (channel + ord('0') if channel < 10 else channel - 10 + ord('A'))
            self.lastchannel = channel

        if time == self.lasttime:
            t = ' ' * 10
        else:
            t = "%8dms" % (time * 1000)
            self.lasttime = time

        logline = "%s %s %s %s" % (t, s, c, str)

        if severity >= self.stderr_min:
            try:
                print >> sys.stderr, logline
            except IOError:
                pass

        if self.logfile and severity >= self.log_min:
            print >> self.logfile, logline
            self.logfile.flush()

logger = Logger()
