# Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
#
# This file is part of the Opera web browser.
# It may not be distributed under any circumstances.

import dispatcher
import feedback
import log

class TestManager(object):

    def __init__(self, tests, repetitions, parallelism):
        self.tests = tests
        self.repetitions = repetitions
        self.pos = 0
        self.completed = 0
        self.failures = 0
        self.processes = [None] * parallelism
        self.tasks = [None] * parallelism
        self.results = [None] * len(tests)
        self.running = 0
        self.feedbackServer = None

    def start(self, spawner):
        self.running = min(self.repetitions * len(self.tests), len(self.processes))
        self.feedbackServer = feedback.FeedbackServer(self.running)
        for i in xrange(self.running):
            self.processes[i] = spawner(i, self, self.feedbackServer)
            self.processes[i].start()

    def onTestingDone(self, id, result):
        self.results[self.tasks[id]] = (self.results[self.tasks[id]] or 0) + result / self.repetitions
        self.tasks[id] = None
        self.completed += 1
        self.reportProgress()
        self.onReady(id)

    def onTestingFailed(self, id, error):
        self.tasks[id] = None
        self.completed += 1
        self.failures += 1
        self.reportProgress()
        if self.haveMore():
            self.processes[id].reset()
        else:
            self.processes[id].cleanup()

    def onNotReady(self, id):
        log.error('Inferior not ready', id)
        self.cleanup

    def onTerminated(self, id):
        if self.haveMore():
            log.error('Premature termination', id)
        self.processes[id] = None
        self.running -= 1
        if self.running == 0:
            if self.haveMore():
                log.error('Test run failed')
                dispatcher.the.stop(1)
            else:
                if self.failures:
                    log.warning("Test run completed, %d failures" % self.failures)
                else:
                    log.info('Test run completed')
                dispatcher.the.stop(0)

    def onReady(self, id):
        if self.haveMore():
            task = self.nextTest()
            self.tasks[id] = task
            self.processes[id].startTest(self.tests[task])
        else:
            self.processes[id].cleanup()

    def reportProgress(self):
        eta = int(dispatcher.the.getTime() * (self.repetitions * len(self.tests) - self.completed) / self.completed)
        log.info("Tested %d/%d, pass %d/%d, ETA %02d:%02d:%02d" %
                 ((self.completed - 1) % len(self.tests) + 1, len(self.tests),
                  (self.completed - 1) / len(self.tests) + 1, self.repetitions,
                  eta / 3600, eta % 3600 / 60, eta % 60))

    def haveMore(self):
        return self.pos < self.repetitions * len(self.tests)

    def nextTest(self):
        res = self.pos % len(self.tests)
        self.pos += 1
        return res

    def getResults(self):
        return self.results

    def cleanup(self):
        for process in self.processes:
            if process:
                process.cleanup()
