"""JIS 0208 Table builder
"""

# Copyright Opera software ASA, 2003-2012.  Written by Eddy.
from basetable import Table, textTable
from tableutils import unhex, NON_UNICODE, sparseTable, twoDict
from string import hexdigits

def tojis0208(cp):
    """Maps jis0208.txt's first column to second.

    This mapping is somewhat ugly !\n"""
    top, bot = (cp & 0xff00) >> 8, cp & 0xff
    adj = bot < 159
    assert adj in (0, 1)

    if top < 160: top = top - 112
    else: top = top - 176
    top = (top << 1) - adj

    if not adj: bot = bot - 126
    elif bot < 128: bot = bot - 31
    else: bot = bot - 32

    return (top << 8) | bot

def findFirstOf(s, *args):
    """In the given string, s, find the first occurence of any other argument.

    Return the index of the first occurance, or -1 if none found.
    """
    try: return min(filter(lambda i: i >= 0, map(lambda a: s.find(a), args)))
    except ValueError: return -1

def parseHTMLtable (src):
    """Return a two-dimensional list containing the contents of a HTML table.

    The HTML table is parsed from the given file object, and the resulting
    list of lists is returned. HTML comments are discarded, case is normalized
    (lowercase), and leading/trailing whitespace removed.

    Constraints:
    - Only <TR> and <TD> tags are considered. Multiple tables will be collapsed
      into the same list of lists
    - Tags (not elements) must not span multiples lines; i.e. if line contains
      contains '<TR', the closing '>' must be on the same line.
    - All <TR> and <TD> tags must be correctly closed and nested. No missing
      tags.
    """
    table = [] # List of lists. 1st level - table rows, 2nd level - table cells
    currentRow = [] # Current 2nd-level list under construction
    currentCell = ''
    States = ('beforeTR', 'beforeTD', 'withinTD', 'withinComment')
    state = 'beforeTR'
    prevState = None # Keep track of previous state when state == withinComment
    withinComment = False
    for line in src:
        line = line.lower().strip() # Normalize lines

        while line: # Keep going until line is exhausted
            assert state in States

            if state == 'beforeTR':
                i = findFirstOf(line, '<tr', '<!--')
                if i >= 0 and line[i:i + 3] == '<tr':
                    line = line[i + 3:].split('>', 1)[1] # Discard up to '>'
                    state = 'beforeTD'
                    currentRow = [] # Start new row
                elif i >= 0 and line[i:i + 4] == '<!--':
                    line = line[i + 2:] # Discard up to '<!'
                    prevState = state
                    state = 'withinComment'
                else: break # No <tr> on this line. Go to next line
            if state == 'beforeTD':
                # Handle both new cell, and end of row
                i = findFirstOf(line, '</tr>', '<td', '<!--')
                if i >= 0 and line[i:i + 5] == '</tr>':
                    line = line[i + 5:] # Discard '</tr>'
                    state = 'beforeTR'
                    table.append(currentRow) # Finalize current row
                elif i >= 0 and line[i:i + 3] == '<td':
                    line = line[i + 3:].split('>', 1)[1] # Discard up to '>'
                    state = 'withinTD'
                    currentCell = '' # Start new cell
                elif i >= 0 and line[i:i + 4] == '<!--':
                    line = line[i + 2:] # Discard up to '<!'
                    prevState = state
                    state = 'withinComment'
                else: break # No <td> or </tr> on this line. Go to next line
            if state == 'withinTD':
                i = findFirstOf(line, '</td>', '<!--')
                if i >= 0 and line[i:i + 5] == '</td>':
                    contents, line = line.split('</td>', 1)
                    currentCell = currentCell + contents
                    state = 'beforeTD'
                    currentRow.append(currentCell) # Finalize current cell
                elif i >= 0 and line[i:i + 4] == '<!--':
                    line = line[i + 2:] # Discard up to '<!'
                    prevState = state
                    state = 'withinComment'
                else: # Entire line is within cell
                    currentCell = currentCell + line
                    break # Go to next line
            if state == 'withinComment':
                i = findFirstOf(line, '-->')
                if i >= 0 and line[i:i + 3] == '-->': # Found end of comment
                    line = line[i + 3:] # Discard up to '-->'
                    state = prevState
                else: # Entire line is within comment
                    break # Go to next line
    return table

def createChecker(*args):
    """Create and return a function for checking character conformance.

    Each argument to this creator shall be a string of characters. The created
    function shall take one string parameter, s, and check whether the first
    character of s is present within the first argument to this creator.
    It shall then check the second character of s against the second string
    passed to this creator. This continues for as many arguments as is given
    to this creator. If s is too short, or otherwise fails the check, False
    shall be returned from the created function. If the check succeeds, True
    shall be returned.
    """
    def checker(s):
        if len(s) < len(args): return False
        for c, arg in zip(s, args):
            if c not in arg: return False
        return True
    return checker

class firstDict (dict):
    """A dictionary that ignores attempts to over-write.

    The JIS-0208 table requires us to read two tables of data,
    ignoring any later entry that over-writes any earlier (see
    CORE-43416).  This implements the needed mapping, for deployment
    by twoDict.\n"""
    __setit = dict.__setitem__
    def __setitem__(self, key, value):
        # Ignore if we already have an entry for this key:
        try: old = self[key]
        except KeyError: self.__setit(key, value)

class jis0208Table (sparseTable, textTable):
    source = textTable.source
    __upinit = textTable.__init__

    def __init__(self, name, emojis):
        assert name == 'jis-0208', name
        self.__upinit(name, emojis)
        self.depend('jis0208.py') # this file

        # Order of source() calls must match that of entries in __parse:
        self.source('cp932')
        self.source('jis0208')
        self.__parse = (self.cparse, self.jparse)

        # only handling of emojis remain ...

        # The following nested functions are parsers for the various emoji types
        def get_imode(src, fwd):
            """Parser for sources/imode-emoji.html

            Derived from old sources/imode-emoji-makelist.pl,
            itself derived from sources by NTT DoCoMo."""

            issjis = createChecker('Ff', '89', hexdigits, hexdigits)
            ispua  = createChecker('Ee', '67', hexdigits, hexdigits)

            for row in parseHTMLtable(src):
                # Consider only rows with 7 cells where cell 3 is a valid sjis
                if len(row) != 7 or not issjis(row[2]):
                    continue
                assert issjis(row[2]), 'No Shift-JIS data'
                sjis = row[2][:4]

                if ispua(row[3]): pua = row[3][:4]
                elif ispua(row[4]): pua = row[4][:4]
                else: assert None, 'Missing Unicode PUA data for %s' % (sjis)

                cp = unhex(sjis)
                if cp > 32000:
                    print "fwd[%s] = %s" % (tojis0208(cp), unhex(pua))
                    fwd[tojis0208(cp)] = unhex(pua)

        def get_kddi(src, fwd):
            """Parser for sources/kddi-emojis.html. """

            issjis = createChecker('Ff', '3467',   hexdigits, hexdigits)
            ispua  = createChecker('Ee', '45AaBb', hexdigits, hexdigits)

            for row in parseHTMLtable(src):
                # Consider only rows with 4 cells where cell 3 is a valid sjis
                if len(row) != 4 or not issjis(row[2]):
                    continue
                assert issjis(row[2]), 'No Shift-JIS data'
                sjis = row[2][:4]

                if ispua(row[3]): pua = row[3][:4]
                else: assert None, 'Missing Unicode PUA data for %s' % (sjis)

                cp = unhex(sjis)
                if cp > 32000:
                    print "fwd[%s] = %s" % (tojis0208(cp), unhex(pua))
                    fwd[tojis0208(cp)] = unhex(pua)

        def get_kddi_spec_chars(src, fwd):
            """Parser for sources/kddi-spec_chars.html. """

            issjis = createChecker('58Ff',  '17Cc', hexdigits, hexdigits)
            ispua  = createChecker('2AaBb', hexdigits)

            for row in parseHTMLtable(src):
                # Strip leading '0x' from all cells
                row = map(lambda cell: cell.lstrip('0x'), row)
                # Consider only rows with at least 4 cells where cell 3 is sjis
                if len(row) < 4 or not issjis(row[2]):
                    continue
                assert issjis(row[2]), 'No Shift-JIS data'
                sjis = row[2][:4]

                if ispua(row[1]): pua = row[1][:4]
                else: assert None, 'Missing Unicode PUA data for %s' % (sjis)

                cp = unhex(sjis)
                if cp > 32000:
                    print "fwd[%s] = %s" % (tojis0208(cp), unhex(pua))
                    fwd[tojis0208(cp)] = unhex(pua)

        # Invoke above parser functions as needed
        if 'imode' in emojis:
            Table.source(self, 'imode-emoji.html', get_imode)

        if 'kddi' in emojis:
            Table.source(self, 'kddi-emojis.html', get_kddi)

        if 'kddi_spec_chars' in emojis:
            Table.source(self, 'kddi-spec_chars.html', get_kddi_spec_chars)
    # end of __init__()

    def cparse(self, row):
        # Parsing cp932.txt, in two-column format:
        cp = unhex(row[0])
        if cp > 32000:
            return tojis0208(cp), unhex(row[1])

        raise ValueError # i.e., ignore this line

    def jparse(self, row):
        # Parsing jis0208.txt, in three-column format:
        cp = unhex(row[0]) # or row[1] ?
        if cp > 32000: # do we want this test ?  or what test in its place ?
            assert tojis0208(cp) == unhex(row[1])
            return tojis0208(cp), unhex(row[2])

        raise ValueError # i.e., ignore this line

    __updigest = textTable.digest
    def digest(self, src, fwd):
        # pop suitable parser off tuple:
        self.parse, self.__parse = self.__parse[0], self.__parse[1:]
        self.__updigest(src, fwd)

    def build_forward(self, out, endian):
        maps = self.load(twoDict(firstDict))
        if out is not None:
            out.open(endian)
            try: self.blockout(out, range(0x21, 0x7f), range(0x21, 0x98), maps.fwd.get)
            finally: out.close()

        return maps.rev.items()

    dbcs_bounds = 0x4e00, 0x9fa1

del textTable
