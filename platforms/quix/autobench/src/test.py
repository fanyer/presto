# Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
#
# This file is part of the Opera web browser.
# It may not be distributed under any circumstances.

import time

import dispatcher
import log

class SPARTANTest(object):

    def __init__(self, url, timeout):
        self.url = url
        self.timeout = timeout
        self.name = url

    def onStart(self, process):
        process.setUserJS("""
(function() {
    if (top == window && !window.opener) {
        window.opener = {
            rr: function(res) {
                for (var i = 1; i < arguments.length; i++)
                    res += arguments[i];
                location = '%(url)s/' + res;
            }
        }
    };
    var m = location.href.match(/^http:\/\/localhost\/\?result=(.*)$/);
    if (m)
        location = '%(url)s/' + m[1];
})();
""" % {'url': process.makeURL('result')})
        if process.pushURL(self.url):
            dispatcher.the.setAlarm(self.timeout, self, process)
        else:
            process.onTestingFailed('Test communication failed')

    def onFeedback(self, process, reply):
        dispatcher.the.removeAlarm(self, process)
        if reply == 'result/undefined':
            process.onTestingFailed('Test reported failure')
        elif reply.startswith('result/'):
            process.onTestingDone(sum([float(s) for s in reply[7: ].split(',')]))
        else:
            process.onTestingFailed('Test communication failed')

    def onTimeout(self, process):
        process.onTestingFailed('Test timed out')

    def abort(self, process):
        dispatcher.the.removeAlarm(self, process)
