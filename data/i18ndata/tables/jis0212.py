"""JIS 0212 Table builder
"""

# Copyright Opera software ASA, 2003-2004.  Written by Eddy.
from basetable import Table, textTable
from tableutils import unhex, NON_UNICODE, sparseTable

class jis0212Table (sparseTable, textTable):
    source = textTable.source
    __upinit = textTable.__init__

    def __init__(self, name, emojis):
        assert name == 'jis-0212', name
        self.__upinit(name, emojis)
        self.source('jis0212')
        self.depend('jis0212.py') # this file
    # end of __init__()

    def parse(self, row):
        k, v = unhex(row[0]), unhex(row[1])
        if k < 0xa1: raise ValueError
        if k == 0x2237: v = 0xFF5E # CORE-45158
        return k, v

    def build_forward(self, out, endian):
        # Maybe we should use a twoDict(), in case sources conflict
        fwd = self.load({}) # but old-form did it by reversing.
        if out is not None:
            out.open(endian)
            try: self.blockout(out, range(0x21, 0x7f), range(0x21, 0x98), fwd.get)
            finally: out.close()

        if 1: # faithful mimic of old code
            fwd = fwd.items()
            fwd.sort()
            bok = {}
            for k, v in fwd:
                try: bok[v]
                except KeyError: bok[v] = k
                # else: print 'ignored conflicting %x: ' % v, bok[v], k

            return bok.items()
        else: # this would give a fuller account of what's available.
            return map(lambda (k,v): (v,k), fwd.items())

    dbcs_bounds = 0x4e00, 0x9fa1

del textTable
