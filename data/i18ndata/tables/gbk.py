"""GBK and GB-18030 Table builder

This data is retrieved from the GB-18030 table, since GB-18030 is a
superset of GBK and GB2312. """

# Copyright Opera software ASA, 2003-2012.  Written by Eddy.
from basetable import Table
from tableutils import unhex, NON_UNICODE, sparseTable
import re

class _gbkTable (Table):
    __upinit = Table.__init__
    def __init__(self, name, emojis):
        if name[-6:] != '-table': name = name + '-table'
        self.__upinit(name, emojis)
        self.source('gb-18030-2000.xml', self.digest)
        self.depend('gbk.py')

class gbkTable (sparseTable, _gbkTable):
    __init__ = _gbkTable.__init__
    def digest(self, src, fwd, recomp=re.compile):
        filter = recomp('<a u="([0-9A-F]{4})" b="([0-9A-F]{2}) ([0-9A-F]{2})"/>')
        for line in src:
            bits = filter.search(line)
            if bits is None: continue

            uni = unhex(bits.group(1)) # 0x7250
            if 0xe000 <= uni < 0xf900: continue
            assert not (0x9fa6 <= uni <= 0xa000), 'added something to reverse table'
            gbk = unhex(bits.group(2) + bits.group(3)) # a0 a3
            fwd[gbk] = uni

    def build_forward(self, out, endian):
        assert self.name == 'gbk-table', self.name
        fwd = self.load({})
        res = fwd.items() # done before bodge so as to leave it out of rev
        # DSK-100985: =A3A0 is used as a de-facto space character
        fwd[0xA3A0] = 0x3000

        # In the 0xFF x 0xFF character square, GBK only uses character
        # positions in the 0x81 - 0xFE (columns) and 0x40 - 0xFE (rows)
        # area.  We exploit this to shrink the table.
        if out is not None:
            out.open(endian)
            try: self.blockout(out, range(0x40, 0xff), range(0x81, 0xff), fwd.get)
            finally: out.close()

        return map(lambda (k,v): (v,k), res)

    dbcs_bounds = 0x4e00, 0x9fa6

class gbkOffsetTable (_gbkTable):
    """Create GB18030 mapping for BMP.

    Map the four-byte GB18030 sequences encoding the Unicode Basic
    Multilingual Plane, creating a pair table (codepoint, seqpoint), where
    "codepoint" is a Unicode BMP codepoint and "seqpoint" is a linear
    sequence point, described below, both stored as sixteen-bit values.

    The sequence point is calculated as a linear mapping of the encoding
    space [81,30,81,30] -- [FE,39,FE,39]. Since the first and third byte are
    in the range [0x81,0xFE] they can encode 126 different values each, and
    the second and fourth in [0x30,0x39] they can encode 10 values each. We
    can thus encode 10 * 10 * 126 * 126 = 1587600 values ([0..1587599]).

    The codespace [81,30,81,30] -- [84,31,A4,39] is used to encode the BMP.
    It can be encoded using the sequence point range [0..39419], which can be
    encoded in sixteen bits.

    Each entry in the BMP table defines the start of a linear relationship
    between the Unicode codepoint and the GB18030 code. The table is used for
    converting both to and from GB18030, as both columns are sorted by
    increasing value simultaneously."""
    def digest(self, src, fwd, recomp=re.compile):
        """Load the four-byte code offsets from the GB 18030 table.
        """
        one = recomp('u="(....)" b="(..) (..) (..) (..)"')
        many = recomp(
            '<range\s+uFirst="(....)"\s+uLast="(....)"\s+bFirst="(..) (..) (..) (..)"')

        for line in src:
            bits = one.search(line)
            if bits is None:
                # Range of codepoints
                bits = many.search(line)
                if bits is None: continue
                data = map(lambda i, g=bits.group: unhex(g(i)), range(1, 7))
                lo, hi, data = data[0], data[1], data[2:]

            else:
                # Single codepoint: treat as a range of length 1
                data = map(lambda i, g=bits.group: unhex(g(i)), range(1, 6))
                lo = hi = data[0]
                data = data[1:]

            # Calculate the "seqpoint" from the GB four-byte encoding:
            point = (((data[0] - 0x81) * 10 +
                      data[1] - 0x30) * 126 +
                     data[2] - 0x81) * 10 + data[3] - 0x30

            while lo <= hi:
                fwd[lo] = point
                lo, point = lo + 1, point + 1

    def build(self, endian, dir, force, want):
        assert self.name == 'gb18030-table' and want(self.name), self.name
        outs, date = self.prepare(dir, want, force, self.name)

        if date is None:
            fwd = self.load({})
            row = fwd.items()
            row.sort()

            out = outs[self.name]
            # Since codepoint/seqpoint mapping comes in runs, only
            # record the points where the difference changes.
            (k, v), row = row[0], row[1:]
            out.open(endian)
            try:
                out.emit('HH', k, v)
                gap = k - v
                for k, v in row:
                    if v + gap != k:
                        out.emit('HH', k, v)
                        gap = k - v

            finally: out.close()

        return outs

del Table, re, _gbkTable
