"""Unicode blocks Table builder
"""

# Copyright Opera software ASA, 2003-2010.  Written by Eddy.
from basetable import Table
from tableutils import unhex
import string

class blockTable (Table):
    __upinit = Table.__init__
    def __init__(self, name, emojis):
        assert name == 'uniblocks'
        self.__upinit(name, emojis)
        # uniblocks.txt contains the unicode block ranges. It is generated
        # by running build_uniblocks.py on os2.html, which is the HTML
        # documentation of the OS/2-table in the OpenType specification
        # ( http://www.microsoft.com/typography/otspec/os2.htm )
        self.source('uniblocks.txt', self.digest)
        self.depend('unibits.py')

    def digest(self, src, row):
        for line in src:
            try:
                num, lo, hi, name = tuple(map(string.strip, string.split(line, ';')))
                if num: num = int(num)
                else: num = 128
                lo, hi = unhex(lo), unhex(hi)
                # Unicode values go up to 0x10ffff, we can encode up to 0xffffff
                assert lo < 0xffffff > hi
            except (ValueError, IndexError, AssertionError):
                print 'skipping bogus line:', line
                pass
            else:
                row.append((num, lo, hi))

    def build(self, endian, dir, force, want):
        assert want(self.name), 'How come this table got asked for ?'
        outs, date = self.prepare(dir, want, force, self.name)

        if date is None:
            row = self.load([])
            row.append((0, 0, 0))

            out = outs[self.name]
            out.open(endian)
            try:
                for num, start, end in row:
                    # Encode range as 8-bit plane plus 16-bit start and end
                    # within that plane. For ranges that span several planes,
                    # we split them into several entries.
                    start_hi, start_lo = divmod(start, 0x10000)
                    end_hi,   end_lo   = divmod(end,   0x10000)
                    for plane in range(start_hi, end_hi + 1): # Loop over planes
                        plane_start, plane_end = start_lo, end_lo
                        if plane != start_hi: # Not the first plane
                            plane_start = 0x0000
                        if plane != end_hi: # Not the last plane
                            plane_end = 0xffff
                        assert 0 <= plane       <= 0xff
                        assert 0 <= plane_start <= 0xffff
                        assert 0 <= plane_end   <= 0xffff
                        out.emit('BBHH', num, plane, plane_start, plane_end)
            finally: out.close()

        return outs
