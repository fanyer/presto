"""Hong Kong's SCS mapping.

Only handled, in fact, as a pair of reversed tables (forward mapping is computed
as needed on the fly by Opera) and a Big5 compatibility table. """

# Copyright Opera software ASA, 2003-2004.  Written by Eddy.
from basetable import textTable
from tableutils import unhex

class hkscsTable (textTable):
    __upinit = textTable.__init__
    def __init__(self, name, emojis):
        assert name[:-1] == 'hkscs-plane-'
        self.__upinit(name, emojis)
        self.source('big5hkscs')
        self.depend('hkscs.py')

    def parse(self, row):
        if len(row) != 3: raise ValueError # not a data line
        byte, uni = unhex(row[2]), unhex(row[1])
        if byte == 0 or 0xe000 <= uni < 0xf900: raise ValueError # skip this line
        if 0x10000 <= uni < 0x20000 or 0x30000 <= uni:
            print 'Unrecognised codepoint:', row[3], '(for %s)' % row[0]
            raise ValueError # skip these, too
        return uni, byte # we're actually building a reversed table !

    comment_delimiter = None
    def build(self, endian, dir, force, want):
        outs, date = self.prepare(dir, want, force,
                                  'hkscs-plane-0', 'hkscs-plane-2')

        if date is None:
            lo = self.load({}) # both tables, jumbled together
            hi = {} # split between planes
            for k in filter(lambda x: x >= 0x10000, lo.keys()):
                assert 0x20000 <= k < 0x30000
                hi[k - 0x20000] = lo[k]
                del lo[k]

            for (k, b) in (('hkscs-plane-0', lo), ('hkscs-plane-2', hi)):
                try: out = outs[k]
                except KeyError: pass
                else: self.writepairs(outs[k], endian, b.items())

        return outs

class hkscsCompatTable (textTable):
    __upinit = textTable.__init__
    def __init__(self, name, emojis):
        assert name == 'hkscs-compat'
        self.__upinit(name, emojis)
        self.source('big5cmp')
        self.depend('hkscs.py')

    def parse(self, row):
        if len(row) == 2 and len(row[0]) == 4 == len(row[1]):
            byte, uni = unhex(row[0]), unhex(row[1])
            if 0 <= byte < 0x10000: return byte, uni

        raise ValueError # ignore this line

    comment_delimiter = None
    def build(self, endian, dir, force, want):
        assert want(self.name), 'how come this table got asked for at all ?'
        outs, date = self.prepare(dir, want, force, self.name)

        if date is None:
            self.writepairs(outs[self.name], endian, self.load({}).items())

        return outs
