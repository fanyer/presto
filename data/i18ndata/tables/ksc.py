"""KS X 1001:1992 (previously known as KSC 5601) Table builder
"""

# Copyright Opera software ASA, 2003-2004.  Written by Eddy.
from basetable import textTable
from tableutils import unhex, sparseTable, hexTable

class kscTable (sparseTable, textTable, hexTable):
    source = textTable.source
    __upinit = textTable.__init__
    def __init__(self, name, emojis):
        assert name in ('ksc5601-table', 'ksc5601'), name
        self.__upinit('ksc5601-table', emojis)
        self.source('cp949')
        self.depend('ksc.py')

    __upparse = hexTable.parse
    def parse(self, row):
        k, v = self.__upparse(row)
        if k < 0xa1: raise ValueError
        return k, v

    def build_forward(self, out, endian):
        fwd = self.load({})

        # in the 0xFF x 0xFF character square, KS C 5601 only uses these
        # character positions:
        # Rows 0x81 - 0xC6: Columns 0x41 - 0x5A, 0x61 - 0x7A, 0x81 - 0xFE
        # Rows 0xC7 - 0xFD: Columns 0xA1 - 0xFE
        # We exploit this to shrink the table.

        if out is not None:
            out.open(endian)
            try:
                self.blockout(out,
                              range(0x41, 0x5b) + range(0x61, 0x7b) + range(0x81, 0xff),
                              range(0x81, 0xc7), fwd.get)
                self.blockout(out, range(0xa1, 0xff), range(0xc7, 0xfe), fwd.get)
            finally: out.close()

        return map(lambda (k,v): (v,k), fwd.items())

    # Note that since KS C 5601 contains all characters between U+AC00 and
    # U+D7A3, and most from U+4E00 -> U+A000 we make two mapping tables (as for
    # all other DBCS reverse mappings):
    #  - one ordinary lookup table for U+AC00 -> U+D7A3
    #  - one table of (Unicode cp, KS C 5601 cp) records, each 4 bytes
    dbcs_bounds = 0xac00, 0xd7a4
