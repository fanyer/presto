"""Big5 Table builder.

Big5-2003 unifies Microsoft's cp950 with earlier extensions. We still read
Unicode.org's obsolete big5.txt since it does contain some alternative mappings
not listed in cp950 or the cp950-based Big5-2003 mapping table that is at our
disposal. The range C6A1--C7FE has been removed from Unicode.org's table due
to incompatibilities with Big5-2003.
"""

# Copyright Opera software ASA, 2003-2006.  Written by Eddy.
from basetable import textTable
from tableutils import unhex, NON_UNICODE, twoDict, sparseTable

class big5Table (sparseTable, textTable):
    source = textTable.source
    __upinit = textTable.__init__
    def __init__(self, name, emojis):
        assert name in ('big5-table', 'big5'), name
        self.__upinit('big5-table', emojis)
        # NB: whichever source comes second may over-write some of the other
        self.source('big5') # unicode.org's table (no self-conflicts)
        self.source('big5-2003') # big5-2003 based on microsoft table (may over-write some)
        # hmm ... and that may mean reverse mapping *is* worth load() producing ...
        self.depend('big5.py')
        self.depend('tableutils.py')

    def parse(self, row):
        big, uni = unhex(row[0]), unhex(row[1])
        if uni == NON_UNICODE or big < 0xa1: raise ValueError # ignore this line
        return big, uni

    def build_forward(self, out, endian):
        maps = self.load(twoDict())

        if out is not None:
            out.open(endian)
            # In the 0xFF x 0xFF character square, Big5 only uses character
            # positions in the 0x40 - 0xFE (columns) and 0xA1 - 0xF9 (rows)
            # area.  We exploit this to shrink the table somewhat.
            try: self.blockout(out, range(0x40, 0xff), range(0xa1, 0xfa), maps.fwd.get)
            finally: out.close()

        return maps.rev.items()

    # Because of the empty areas in the Big5 mapping, we make two
    # mapping tables (as for all other DBCS reverse mappings).
    #  - one ordinary lookup table for U+4E00 -> U+A000
    #  - one table of (Unicode cp, Big5 cp) records, each 4 bytes
    dbcs_bounds = 0x4e00, 0x9fa5
