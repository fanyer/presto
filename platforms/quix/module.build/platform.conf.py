
import sys

class AutoPlatform(default.AutoPlatform):

    def isLinux(self):
        return sys.platform.startswith('linux')

    def isFreeBSD(self):
        return sys.platform.startswith('freebsd')
