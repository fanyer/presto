# Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
#
# This file is part of the Opera web browser.
# It may not be distributed under any circumstances.

import os
import copy
import tempfile
import shutil
import subprocess
import signal

import dispatcher
import process
import log

class OperaProcess(process.Process):

    def __init__(self, id, testManager, feedbackServer, wrapper, width, height, xserver=None):
        process.Process.__init__(self)
        self.id = id
        self.testManager = testManager
        self.feedbackServer = feedbackServer
        self.wrapper = wrapper
        self.width = width
        self.height = height
        self.xserver = xserver
        self.test = None
        self.prepare()

    def prepare(self):
        self.profile = tempfile.mkdtemp(prefix='autobench.')

        log.info("Using profile directory %s" % self.profile, self.id)

        open("%s/operaprefs.ini" % self.profile, 'w').write("""
[User Prefs]
Preferences Version=2

; Disable usage report
Enable Usage Statistics=0
Ask For Usage Stats Percentage=0

; Disable browser.js
Browser JavaScript=0

; Disable autoupdate
Level Of Update Automation=0

; Disable video driver blocklist update
UNIX OpenGL Blocklist URL=

; Disable UPnP Discovery
Enable Service Discovery Notifications=0

; Disable fraud checking
Enable Trust Rating=0

; Disable popup blocker
Ignore Unrequested Popups=0

; Open popups in background
Target Destination=2

; Enable our UserJS
User JavaScript=1
User JavaScript File=%(profile)s/userjs/

; Load UserJS even on pages without scripts
Always Load User JavaScript=1

; Restart as usual after forced shutdown
Show Problem Dialog=0

[File Selector]
; Use plain X11
Dialog Toolkit=4

[State]
; Don't show license dialog
Accept License=1

[Install]
; Don't show post-upgrade page
Newest Used Version=99.99.9999

[Auto Update]
; Don't download dictionaries.xml
Dictionary Time=2000000000

[Network]
; Allow redirects to localhost
Allow Cross Network Navigation=1

[Security Prefs]
; Don't contact external OCSP servers
OCSP Validate Certificates=0

[Web Server]
; Disable UPnP
UPnP Enabled=0
UPnP Service Discovery Enabled=0

""" % {'profile': self.profile})

        os.mkdir("%s/dictionaries" % self.profile)
        open("%s/dictionaries/dictionaries.xml" % self.profile, 'w').write('<?xml version="1.0" encoding="UTF-8"?><dictionaries version="1.0"/>')

        os.mkdir("%s/userjs" % self.profile)

    def start(self):
        log.info('Starting inferior', self.id)
        self.test = None
        self.isReset = False
        self.feedbackServer.addChannel(self.id, self)
        geometry = "%dx%d" % (self.width, self.height)
        if self.xserver:
            geometry += "+%d+%d" % (self.width * self.id % self.xserver.width, self.width * self.id / self.xserver.width * self.height)
        args = [self.wrapper,
                '-pd', self.profile,
                '--geometry', geometry,
                '--activetab', self.feedbackServer.makeURL(self.id, 'ready')]
        environ = copy.deepcopy(os.environ)
        environ['LANG'] = 'C'
        environ['OPERA_DEBUGGER'] = 'exec'
        environ['OPASSERT_CONTINUE'] = '1'
        if self.xserver:
            environ['DISPLAY'] = self.xserver.display
        self.onStarted(subprocess.Popen(args, close_fds=True, env=environ, stdout=open('/dev/null', 'w'), stderr=open('/dev/null', 'w')))
        dispatcher.the.setAlarm(60, self, self.onNotReady)

    def makeURL(self, data):
        return self.feedbackServer.makeURL(self.id, data)

    def pushURL(self, url):
        return self.feedbackServer.pushURL(self.id, url)

    def startTest(self, test):
        log.info("Starting test: %s" % test.name, self.id)
        self.test = test
        test.onStart(self)

    def onFeedback(self, data):
        if self.test:
            self.test.onFeedback(self, data)
        elif data == 'ready':
            dispatcher.the.removeAlarm(self, self.onNotReady)
            log.info('Inferior ready', self.id)
            self.testManager.onReady(self.id)

    def onTestingDone(self, result):
        log.info("Testing done: %s (result: %f)" % (self.test.name, result), self.id)
        self.test = None
        self.testManager.onTestingDone(self.id, result)

    def onTestingFailed(self, error):
        log.error("Testing failed: %s (%s)" % (self.test.name, error), self.id)
        self.test = None
        self.testManager.onTestingFailed(self.id, error)

    def reset(self):
        log.info('Trying to reset inferior', self.id)
        self.isReset = True
        if self.feedbackServer.pushURL(self.id, self.feedbackServer.makeURL(self.id, 'ready')):
            dispatcher.the.setAlarm(60, self, self.cleanup)
        else:
            self.cleanup()

    def stop(self):
        if self.test:
            self.test.abort(self)
            self.test = None
        self.feedbackServer.removeChannel(id)
        if self.isrunning():
            log.info('Sending SIGTERM', self.id)
            try:
                os.kill(self.inferior.pid, signal.SIGTERM)
            except OSError:
                pass
            dispatcher.the.setAlarm(300, self, self.kill)

    def kill(self):
        if self.isrunning():
            log.info('Sending SIGKILL', self.id)
            try:
                os.kill(self.inferior.pid, signal.SIGKILL)
            except OSError:
                pass

    def onTermination(self):
        log.info('Inferior terminated', self.id)
        if self.test:
            self.test.abort(self)
            self.test = None
        self.feedbackServer.removeChannel(id)
        shutil.rmtree(self.profile, ignore_errors=True)
        if self.isReset:
            self.prepare()
            self.start()
        else:
            self.testManager.onTerminated(self.id)

    def onNotReady(self):
        self.testManager.onNotReady(self.id)

    def onTimeout(self, callback):
        callback()

    def cleanup(self):
        process.Process.cleanup(self)

    def setUserJS(self, text):
        if text:
            open("%s/userjs/autobench.js" % self.profile, 'w').write(text)
        else:
            try:
                os.unlink("%s/userjs/autobench.js" % self.profile)
            except OSError:
                pass
