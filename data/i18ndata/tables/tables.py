"""Particular details of table generators.

This is the primary junction box which selects a class derived from
Table to handle each given table. """

# Copyright Opera software ASA, 2003-2012.  Written by Eddy.

_odd_sbcs = ("koi8-r", "koi8-u", "ibm866", "ptcp154")

def known():
    """Emits a list of all known table names.

    Or, at least, the ones of interest to charsetlist.charsetsTable.enlist().
    """

    return ("viscii",) + _odd_sbcs + \
           tuple(map(lambda x: 'iso-8859-%d' % x, range(2, 12) + range(13, 17))) + \
           tuple(map(lambda x: 'windows-125%d' % x, range(9)))
# What about other "format A" files: gb2312, jis0212, big5,
# windows-{866,874,932,936,949,950}, ksc5601, ksx1001, cns11643 ?

from charsetlist import charsetsTable
from unibits    import blockTable

from sbcs       import sbcsTable, visciiTable
from jis0212    import jis0212Table
from jis0208    import jis0208Table
from hkscs      import hkscsTable, hkscsCompatTable
from big5       import big5Table
from gbk        import gbkTable, gbkOffsetTable
from ksc        import kscTable
from cns        import cnsTable

table_class = {
    'charsets': charsetsTable,
    'uniblocks': blockTable,

    'viscii': visciiTable,
    'jis-0208': jis0208Table, # the only one that cares about emojis
    'jis-0212': jis0212Table,
    'hkscs-compat': hkscsCompatTable, 'big5': big5Table,
    'gbk': gbkTable, 'gb18030': gbkOffsetTable,
    'ksc5601': kscTable, 'cns11643': cnsTable }

for nom in ('hkscs-plane-0', 'hkscs-plane-2'): table_class[nom] = hkscsTable
for nom in _odd_sbcs: table_class[nom] = sbcsTable

import string
def table_class(name, specials=table_class, find=string.find):
    # Returns a Table for the named charset

    if name[:9] == 'iso-8859-' or name[:8] == 'windows-' or name[:6] == 'x-mac-' or name[:3] == 'mac':
        return sbcsTable

    cut = find(name, '-table')
    if cut < 0: nom = name
    else: nom = name[:cut]

    try: return specials[nom]
    except KeyError:
        raise ValueError('Unrecognised table name, %s,' % name)

def table(name, emojis, klaz=table_class, find=string.find):
    cut = find(name, '-rev')
    if cut >= 0: name = name[:cut]
    return klaz(name)(name, emojis)

del string, table_class
