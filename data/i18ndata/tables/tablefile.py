"""Objects describing files containing (binary) tables.

This should be the only .py which imports struct ...
"""

# Copyright Opera software ASA, 2003.  Written by Eddy.
import struct, os

class tableFile:
    """Objects describing files containing (binary) tables.

    Methods for use by Table()s:
        open(endian) -- opens handle on self.filename
        write([text, ...]) -- writes all args to handle
        emit(fmt, [val, ...]) -- writes values according to fmt
        close() -- closes file handle

    The open() method needs to know endianness so that emit() has a
    chance of success: emit()'s first argument is a struct.pack format
    string, to which either '<' or '>' will be prepended if it does
    not begin with an explicit packing character (in '@=><!').  The
    transcribe() method is provided for use by the describe function,
    below (q.v.). """

    def __init__(self, name, dir,
                 getstat=os.stat, OSError=os.error, pave=os.path.join):
        self.name, self.filename = name, pave(dir, name + '.tbl')
        try: stats = getstat(self.filename)
        except OSError: self.size = self.date = None
        else:
            size = stats[6] # but that's a long; massage
            try: self.size = int(size)
            except OverflowError: self.size = size
            self.date = stats[8]

    def open(self, endian):
        self.__fd = open(self.filename, 'wb')
        self.size, self.__flag = 0, endian

    def close(self):
        self.__fd.close()
        del self.__fd, self.__flag

    def write(self, *args):
        try: fd = self.__fd
        except AttributeError:
            raise IOError('Attempted write when not open')

        map(fd.write, args)
        self.size = reduce(lambda x, y: x+y, map(len, args), self.size)

    def emit(self, fmt, *args):
        self.write(self.__pack(fmt, args))

    # Private:
    def __pack(self, fmt, row, pack=struct.pack):
        if fmt and fmt[0] not in '@=><!':
            fmt = self.__flag + fmt
        return apply(pack, (fmt,) + row)

    # For use by describe, below:
    def transcribe(self, target):
        src = open(self.filename, 'rb')
        try:
            while 1:
                buf = src.read(4096)
                if not buf: break
                target.write(buf)
        finally:
            src.close()

def describe(row, flag, target, pack=struct.pack):
    """Writes a chartable to target for a given sequence of tables.

    Required arguments:
      row -- sequence of (name, tableFile()) pairs to be described
      flag -- one of '<' and '>' indicating endianness (see struct.pack)
      target -- stream to which to write the output
    """

    target.write(pack(flag + 'H', len(row))) # header of the header ;^>
    offset = 0
    for name, tbl in row:
        assert tbl.name == name
        h = len(name)
        target.write(pack('%cLB%dsL' % (flag, h), tbl.size, h, name, offset))
        offset = offset + tbl.size

    # Append each table to the end:
    for name, tbl in row: tbl.transcribe(target)

del os, struct
