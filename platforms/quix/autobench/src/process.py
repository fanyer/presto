# Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
#
# This file is part of the Opera web browser.
# It may not be distributed under any circumstances.

import subprocess
import os
import signal

import dispatcher

class Process(object):

    def __init__(self):
        self.inferior = None

    def onStarted(self, inferior):
        self.inferior = inferior
        dispatcher.the.addChild(inferior.pid, self)

    def onPossibleTermination(self, pid):
        if self.inferior.poll() != None:
            dispatcher.the.removeHandler(self)
            self.inferior = None
            self.onTermination()

    def onTermination(self):
        pass

    def isrunning(self):
        return self.inferior != None

    def stop(self):
        if self.isrunning():
            os.kill(self.inferior.pid, signal.SIGTERM)

    def cleanup(self):
        self.stop()
