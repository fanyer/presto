# Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
#
# This file is part of the Opera web browser.
# It may not be distributed under any circumstances.

import os
import signal
import select
import time

class Dispatcher(object):

    def __init__(self):
        self.children = {}
        self.readers = {}
        self.alarms = []
        self.sigchld_pipe = os.pipe()
        self.retval = None
        self.starttime = time.time()
        self.time = 0
        signal.signal(signal.SIGCHLD, lambda signum, frame: os.write(self.sigchld_pipe[1], 'S'))

    def getTime(self):
        return self.time

    def stop(self, retval):
        self.retval = retval

    def addChild(self, pid, child):
        self.children[pid] = child

    def removeChannel(self, pid):
        if pid in self.children:
            del self.children[pid]

    def addReader(self, fd, reader):
        self.readers[fd] = reader

    def removeReader(self, fd):
        if fd in self.readers:
            del self.readers[fd]

    def setAlarm(self, timeout, handler, data):
        self.alarms.append((self.time + timeout, handler, data))

    def removeAlarm(self, handler, data):
        i = 0
        while i < len(self.alarms):
            if self.alarms[i][1] == handler and self.alarms[i][2] == data:
                del self.alarms[i]
            else:
                i += 1

    def removeHandler(self, handler):
        for pid in self.children.keys():
            if self.children[pid] == handler:
                del self.children[pid]
        for fd in self.readers.keys():
            if self.readers[fd] == handler:
                del self.readers[fd]
        i = 0
        while i < len(self.alarms):
            if self.alarms[i][1] == handler:
                del self.alarms[i]
            else:
                i += 1

    def run(self):
        while self.retval == None and (self.children or self.readers or self.alarms):
            poll = select.poll()
            poll.register(self.sigchld_pipe[0], select.POLLIN)
            for fd in self.readers:
                poll.register(fd, select.POLLIN | select.POLLERR | select.POLLHUP)
            timeout = None
            for alarm in self.alarms:
                if timeout == None or alarm[0] < self.time + timeout:
                    timeout = alarm[0] - self.time
            try:
                if timeout is None:
                    events = poll.poll()
                elif timeout <= 0:
                    events = []
                else:
                    events = poll.poll(timeout * 1000)
            except select.error:
                events = []
            self.time = time.time() - self.starttime
            i = 0
            while i < len(self.alarms):
                if self.alarms[i][0] <= self.time:
                    t, handler, data = self.alarms[i]
                    del self.alarms[i]
                    handler.onTimeout(data)
                else:
                    i += 1
            for fd, event in events:
                if fd == self.sigchld_pipe[0]:
                    os.read(fd, 1)
                    for pid in self.children.keys():
                        self.children[pid].onPossibleTermination(pid)
                elif fd in self.readers:
                    reader = self.readers[fd]
                    del self.readers[fd]
                    if event & select.POLLIN:
                        reader.onReadReady(fd)
                    if event & (select.POLLHUP | select.POLLERR | select.POLLNVAL):
                        reader.onReadError(fd)
        return self.retval

the = Dispatcher()
