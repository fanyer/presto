"""Assorted utilities for table generation.
"""

# Copyright Opera software ASA, 2003-2012.  Written by Eddy.
from basetable import Table, textTable

# unicode bits:
import string
# Convert hex denotation for a number to the number:
def unhex(txt, a2i=string.atoi): return a2i(txt, 16)
NON_UNICODE = 0xFFFD

del string

class twoDict:
    """Forward-and-reverse mapping class.

    Useful in Table.load() when potential conflicts in the input imply
    that it isn't good enough to compute a forward mapping then
    reverse it (e.g. Big5).  Constructor takes an optional argument,
    defaults to the built-in dict which returns an empty {}; it should
    be a callable which, when called with no parameters, returns a
    mapping object in which to store results.\n"""

    def __init__(self, Book=dict):
        self.fwd, self.rev = Book(), Book()

    def __setitem__(self, key, value):
        self.fwd[key], self.rev[value] = value, key

class hexTable (textTable):
    """Sub-classes textTable for the common case where the interesting
    data are the first two columns of the table, each expressed in
    hexadecimal."""
    def parse(self, row): return unhex(row[0]), unhex(row[1])

class byteTable (textTable):
    """Base class for single-byte tables. """
    # wrap load:
    __load = textTable.load
    def load(self, fwd=[]):
        """Load source file, return forward map as list.

        Optional (default=[]) argument is (an initial segment of) the initial
        forward map, to which enough NON_UNICODE entries will be appended to
        make it up to length 256 before we read source, which will over-write
        entries in map.  This is for viscii's sake, since it initialises the
        bottom half of the table.

        Returns the resulting map (which is a list). """
        return self.__load(fwd + [ NON_UNICODE ] * (256 - len(fwd)))

    # over-ride writepairs:
    def writepairs(self, out, endian, pairs):
        pairs.sort()
        out.open(endian)
        try:
            for u, b in pairs: out.emit('HB', u, b)
        finally: out.close()

class sparseTable (Table):
    """Helper (mixin) class for DBCS tables.

    The DBCS mappings have lots of empty areas in them.  To exploit this, we
    emit two reverse mapping tables: a packed one covering the area with few
    gaps and a (codepoint, sourcepoint) table whose records are 4 bytes each
    rather than 2 bytes.
    """

    def build(self, endian, dir, force, want):
        # See Table.build's doc
        if self.name[-6:] == '-table':
            stem = self.name[:-6] + '-rev-table'
        else: stem = self.name + '-rev'
        outs, date = self.prepare(dir, want, force,
                                  self.name, stem + '-1', stem + '-2')

        if date is None:
            lo, hi = self.dbcs_bounds
            one, two = {}, []
            try: out = outs[self.name]
            except KeyError: out = None
            for v, k in self.build_forward(out, endian):
                if lo <= v < hi: one[v] = k
                elif 0 <= v < 0xffff: two.append((v, k))

            try: out = outs[stem + '-1']
            except KeyError: pass
            else:
                out.open(endian)
                try: apply(out.emit, ('<%dH' % (hi - lo),) + tuple(map(lambda i, g=one.get: g(i, 0), range(lo, hi))))
                finally: out.close()
            del one

            try: out = outs[stem + '-2']
            except KeyError: pass
            else:
                two.sort()
                self.writepairs(out, endian, two)

        return outs

    def build_forward(self, out, endian):
        """Load table, save forward table, return reversal pairs.

        Arguments (both required):
          out -- None, for no output, or a tableFile object
          endian -- flag for endianness to be used in output

        This method is only called if the table needs rebuilt; it must
        drive self.load() and return a sequence of pairs (2-tuples) -
        nominally the result of mapping the forward table's items
        through lambda (k,v): (v,k) - save that some massaging may be
        apt (e.g. cns) and it may equally be the items of an actual
        reverse table if, as in big5, one has been computed.

        If out isn't None, this method should, after loading data,
        out.open(endian), write the forward table then out.close()
        before returning. """

        raise NotImplementedError(self.__class__, 'build_forward()')

    def blockout(self, out, cols, rows, get, offset=0, default=NON_UNICODE):
        """Helper function for build_forward()

        Required arguments:
          out -- open tableFile to which to emit the block
          cols, rows -- ranges, listing relevant column and row indices
          get -- the forward mapping's .get() method

        Optional arguments:
          offset -- added to all keys passed to get (0)
          default -- used as second parameter to get (NON_UNICODE)

        Each key passed to get() will be (offset + c + 256 * r) for
        some c in cols and r in rows.

        These character sets typically have a block within the unicode
        table outside which they have no entries; it thus helps to
        them write out in blocks. """

        fmt = '%dH' % len(cols)
        for row in rows:
            apply(out.emit, (fmt,) +
                  tuple(map(lambda i, g=get, off= offset + row * 256, d=default:
                            g(i + off, d), cols)))

