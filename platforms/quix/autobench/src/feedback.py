# Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
#
# This file is part of the Opera web browser.
# It may not be distributed under any circumstances.

import socket
import errno
import re

import dispatcher

class FeedbackServer(object):

    def __init__(self, backlog):
        self.listener = socket.socket()
        self.port = 50000
        self.conn_read = {}
        self.conn_write = {}
        self.channels = {}
        while True:
            try:
                self.listener.bind(('127.0.0.1', self.port))
                break
            except socket.error, (err, errstr):
                if err != errno.EADDRINUSE:
                    raise
                self.port += 1
        self.listener.listen(backlog)
        dispatcher.the.addReader(self.listener.fileno(), self)

    def makeURL(self, id, data):
        return "http://localhost:%d/%d/%s" % (self.port, id, data)

    def addChannel(self, id, handler):
        self.channels[id] = handler

    def removeChannel(self, id):
        if id in self.conn_write:
            del self.conn_write[id]
        if id in self.channels:
            del self.channels[id]

    def onReadReady(self, fd):
        if fd == self.listener.fileno():
            conn, address = self.listener.accept()
            self.conn_read[conn.fileno()] = conn
            dispatcher.the.addReader(conn.fileno(), self)
            dispatcher.the.addReader(self.listener.fileno(), self)
        else:
            request = self.conn_read[fd].recv(4096)
            m = re.match(r'GET /(\d+)/([^ ]+) HTTP.*', request)
            if m and int(m.group(1)) in self.channels:
                id = int(m.group(1))
                self.conn_write[id] = self.conn_read[fd]
                del self.conn_read[fd]
                self.channels[id].onFeedback(m.group(2))
            else:
                self.conn_read[fd].send("HTTP/1.1 400 Bad request\r\nConnection: Close\r\n\r\n")
                self.conn_read[fd].close()
                del self.conn_read[fd]

    def onReadError(self, fd):
        del self.conn_read[fd]

    def pushURL(self, id, url):
        if id in self.conn_write:
            self.conn_write[id].send("HTTP/1.1 302 Found\r\nConnection: Close\r\nLocation: %s\r\n\r\n" % url)
            self.conn_write[id].close()
            del self.conn_write[id]
            return True
        else:
            return False
