# Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
#
# This file is part of the Opera web browser.
# It may not be distributed under any circumstances.

import subprocess
import sys
import os.path

import process

class VNCServer(process.Process):

    def __init__(self, width, height):
        process.Process.__init__(self)
        self.width = width
        self.height = height
        self.display = None
        self.inferior = None

    def allocateDisplay(self):
        n = 0
        while os.path.exists("/tmp/.X%d-lock" % n):
            n += 1
        return n

    def start(self):
        self.display = ":%d" % self.allocateDisplay()
        self.onStarted(subprocess.Popen(['Xvnc', self.display,
                                        '-nolisten', 'tcp',
                                         '-localhost',
                                         '-SecurityTypes', 'None',
                                         '-geometry', "%dx%d" % (self.width, self.height),
                                         '-depth', '24',
                                         '-pixelformat', 'rgb888',
                                         '-dpi', '96',
                                         '-AlwaysShared'],
                                        stdout=open('/dev/null', 'w'),
                                        stderr=subprocess.STDOUT,
                                        close_fds=True))
