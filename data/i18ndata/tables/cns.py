"""CNS 11643 Table builder
"""

# Copyright Opera software ASA, 2003-2004.  Written by Eddy.
from basetable import textTable
from tableutils import unhex, NON_UNICODE, sparseTable

class cnsTable (sparseTable, textTable):
    source = textTable.source
    __upinit = textTable.__init__
    def __init__(self, name, emojis):
        assert name in ( 'cns11643-table', 'cns11643' ), name
        self.__upinit('cns11643-table', emojis)
        self.source('cns11643')
        self.depend('cns.py')

    def parse(self, row):
        return unhex(row[0]), unhex(row[1])

    def build_forward(self, out, endian):
        fwd = self.load({})

        # CNS-11643 consists of 13 planes (0x1 - 0xE) of characters allocated in
        # a 0x21 - 0x7D (rows) by 0x21 - 0x7E (cols) square.  We exploit this to
        # shrink the table somewhat.  Only planes 1, 2 and E are actually
        # populated, allowing us to shrink the table even further.

        if out is not None:
            out.open(endian)

            try:
                for plane in (1, 2, 0xe):
                    self.blockout(out, range(0x21, 0x7f), range(0x21, 0x7e),
                                  fwd.get, plane << 16)
            finally: out.close()

        def euctw2short((cp, v)):
            # --- Pack EUC-TW codepoint into a 16-bit value
            # also reverse the (sourcepoint, codepoint) pair in the process
            plane, row, cell = (cp >> 16) & 0xf, (cp >> 8) & 0xff, cp & 0xff
            assert not ((row | cell) & 0x80), 'I hoped they were 7-bit !'
            if plane == 0xe: plane = 3
            return v, cell + (row << 7) + (plane << 14)

        return map(euctw2short, fwd.items())

    # Because of the empty areas in the EUC-TW mapping, we make two mapping
    # tables (as for all other DBCS reverse mappings).
    #  - one ordinary lookup table for U+4E00 -> U+9FA5 to packed CNS 11643
    #  - one table of (Unicode cp, packed CNS 11643 cp) records, each 4 bytes

    dbcs_bounds = 0x4e00, 0x9fa6

del textTable
