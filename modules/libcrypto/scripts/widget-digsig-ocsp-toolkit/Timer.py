#!/usr/bin/python

import time


class Timer:
    start = None
    
    def __init__(self      ): self.reset()
    def reset   (self      ): self.start = time.time()
    def elapsed (self      ): return time.time() - self.start
    def set     (self, time): self.start = time
# class Timer
