import sys
from hob.proto import Config

__all__ = ("UserInterface",
           )

class UserInterface(object):
    verbose_level = 1
    config = None
    exts = None
    cmds = None

    def __init__(self, exts, cmds):
        self.verbose_level = 1
        self.config = Config()
        self._buffers = []
        self._debug = False
        self.pdb = False
        self.exts = exts
        self.cmds = cmds
        from hob import __version__, __version_num__
        self.version = __version__
        self.version_num = __version_num__

    def postmortem(self, traceback=None):
        import pdb
        pdb.post_mortem(traceback)

    def pushbuffer(self):
        self._buffers.append([])

    def popbuffer(self):
        return "".join(self._buffers.pop())

    def write(self, *args):
        if self._buffers:
            self._buffers[-1].extend([str(a) for a in args])
        else:
            for a in args:
                sys.stdout.write(str(a))

    def write_err(self, *args):
        try:
            if not sys.stdout.closed: sys.stdout.flush()
            for a in args:
                sys.stderr.write(str(a))
            # stderr may be buffered under win32 when redirected to files,
            # including stdout.
            if not sys.stderr.closed: sys.stderr.flush()
        except IOError, inst:
            if inst.errno != errno.EPIPE:
                raise

    def interactive(self):
        b = self.config.bool("ui", "interactive", None)
        if b is None:
            return sys.stdin.isatty()
        return b

    def debug(self, *args):
        if self._debug:
            self.write(*args)

    def debugl(self, *args):
        if self._debug:
            self.write(*args)
            self.write("\n")

    def out(self, *args):
        if self.verbose_level > 0:
            self.write(*args)

    def outl(self, *args):
        if self.verbose_level > 0:
            self.write(*args)
            self.write("\n")

    def note(self, *args):
        if self.verbose_level > 1:
            self.write(*args)

    def notel(self, *args):
        if self.verbose_level > 1:
            self.write(*args)
            self.write("\n")

    def warn(self, *args):
        self.write_err(*args)

    def warnl(self, *args):
        self.warn(*args)
        self.write_err("\n")
