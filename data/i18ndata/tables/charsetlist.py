"""The pseudo-table which lists all supported encodings
"""

# Copyright Opera software ASA, 2003-2004.  Written by Eddy.
from basetable import Table
import string

def charset_order(left, right,
                  tests=[(9, lambda x: x[:8] == 'iso-8859'),
                         (4, lambda x: x[:3] == 'utf'),
                         (8, lambda x: x[:7] == 'windows')]):
    # {-1, 0, +1} according as left is {earlier than, same as, later than} right.

    # Jump iso-8859-%d, utf-%d, windows-%d to front:
    for h, test in tests:
        if test(left):
            if test(right): return cmp(int(left[h:]), int(right[h:]))
            else: return -1
        elif test(right): return 1

    # Everything else: lexical order
    return cmp(left, right)

class charsetsTable (Table):
    """Special pseudo-table for the file listing all supported encodings."""

    # doesn't call its .load() so doesn't need to implement .parse()
    def build(self, endian, dir, force, want):
        assert self.name == 'charsets' and want(self.name)
        outs, date = self.prepare(dir, want, force, self.name)
        # ignore date; always rebuild
        assert outs.keys() == [ self.name ]
        out = outs[self.name]

        out.open(endian) # ignore endian
        try: out.write(string.join(self.__tables, '\0'), '\0')
        finally: out.close()

        del self.__tables
        return outs

    def enlist(self, tables, test, version):
        """Special method for chartables.tbl

        Required arguments:
          tables -- list of candidate names of tables
          test -- function testing whether a table is being processed
          version -- Opera version number

        The first two are assumed to be the .keys() and .has_key of a
        dictionary; tables is iterated over, test is expected to be
        cheaper than asking whether a name is in tables. """

        # Algorithmic encodings, always supported
        bok = {'us-ascii': None, 'iso-8859-1': None, 'utf-8': None, 'utf-16': None}
        if version >= 7 and version < 10: bok['utf-32'] = None

        # "SBCS, where table names correspond to charset tags"
        for name in filter(test, tables): bok[name] = None

        # Other encodings, where table names do not correspond to charset tags:
        if test('jis-0208'):
            bok.update({'iso-2022-jp': None, 'shift_jis': None, 'euc-jp': None})
	    if test('jis-0212'):
		bok.update({'iso-2022-jp-1': None})
        if test('ksc5601-table'):
            bok['euc-kr'] = None
            if version >= 8: bok['iso-2022-kr'] = None
        if test('big5-table'):
            bok['big5'] = None
            if test('hkscs-plane-0'): bok['big5-hkscs'] = None
        if test('gbk-table'):
            bok.update({'gbk': None, 'gb2312': None, 'hz-gb-2312': None})
            if test('gb18030-table'): bok['gb18030'] = None
        if test('cns11643-table'):
            bok['euc-tw'] = None
            if test('gbk-table'):
                if version >= 7: bok['iso-2022-cn'] = None
        if test('windows-1254'):
            bok.update({'iso-8859-9': None})

        # Used a dictionary so as to avoid duplicates; now convert to
        # custom-sorted list:
        self.__tables = bok.keys()
        self.__tables.sort(charset_order)
