"""Table base-classes.

Table implementations should import Table or textTable from this module; they
should use its methods, and those of the tableFile()s to which these provide
access, to access functionality of os and struct; they should over-ride its
build method and may wish to tweak others.  See doc strings of classes and
methods below.

Exports (all classes):
  Table -- base-class,
  textTable -- specialisation for *.txt files,

See also: tableutils.py, chrtblgen.py (the main program).
"""

# Copyright Opera software ASA, 2003-2004.  Written by Eddy.
import os, string
from tablefile import tableFile
from checker import Checker

class Table:
    """Base-class for all tables.

    API, listing attributes after the methods that initialises them:
      constructor -- takes one parameter, table name
      .name -- table name
      .date -- date of most recently changed file on which table depends
      build(endian, emojis, dir, force, bok) -- build table(s) if needed

    Toolkit, for use by subclasses:
      before the call to build:
        source(file, handler) -- specify how to load, if needed
        depend([file, ...]) -- announce dependency on named files
        .date -- time of most recend modification to a dependency file
      during the call to build (q.v.):
        prepare(dir, force [, tablename...]) -- sets up tableFiles
        load(self, target) -- loads table into target object

    Note that this is the base for pseudo-tables as well as real tables.
    See also Table, below; which, notably, over-rides source().
    """

    def __init__(self, name, emojis):
        """Constructor for all table generators.

        Required arguments:
          name -- the name of the charset
          emojis -- list of emoji types to include, taken from @emojis, e.g. ['imode', 'kddi']

        Derived class constructors should call this early; they should
        subsequently call .depend(file, ...) with a list of names of files on
        which the table depends. """

        self.name, self.date, self.__sources = name, 0, []
        self.depend('basetable.py') # *every* table depends on this file ...

    def build(self, endian, dir, force, want):
        """Read table data and generate table(s) from it.

        Required arguments:
          endian -- '<' for little-endian, '>' for big-endian
          dir -- name of directory in which to store table file
          force -- if true, table will be rebuilt even if up to date
          want -- function; want(table-name) -> true iff table is wanted

        Returns a dictionary.  If self knows how to build a table whose name
        gets a true result from want, that name should be a key in the
        dictionary, mapped to a tableFile (q.v.) object describing the table.
        The .prepare() method (q.v., below) is designed to prepare such a
        dictionary.  On return from build(), all the indicated tables should
        exist. """

        return {} # derived classes should simply over-ride this method.

    # Methods for use by derived classes
    def depend(self, arg,
               getstat=os.stat, OSError=os.error):
        try: date = getstat(arg)[8]
        except OSError:
            raise IOError('Missing dependency', arg)

        if self.date < date: self.date = date # newest dependency

    def source(self, file, meth, pave=os.path.join):
        """Says what to read and how to build table.

        Arguments (both required):
          file -- basename of the file in sources/, will be joined to sources/.
          meth -- a callable, to be called as meth(src, tgt), see below.

        The callable will only be invoked if self.build decides it's necessary
        to re-build the table.  In that case, it is invoked by self.load() and
        receives the following arguments:
          src -- a readable stream accessing the file
          tgt -- the data structure (usually a dictionary) passed to load.
        """

        src = pave('sources', file)
        self.depend(src)
        self.__sources.append((src, meth))

    def prepare(self, dir, want, force, *names):
        """Initialises a tableFile for each named output table.

        Required arguments:
          dir -- directory to hold output files
          want -- function; want(table-name) -> true iff table is wanted
          force -- whether to rebuild regardless of up-to-dateness

        Each subsequent argument, if any, should be the name of a table whose
        file self.build knows how to generate; these are filtered by want.

        Returns a pair; first member is a dictionary mapping the wanted table
        names to their tableFile objects; second is the date of the oldest of
        these files, or None if a rebuild is required (whether because one of
        the files does not exist, because force was specified, or because one of
        the files is older than some dependency of self).  Table()s should
        generally only be rebuilt if this method returns None as the second
        member of this tuple. """

        bok, row = {}, []
        for nom in filter(want, names):
            bok[nom] = fil = tableFile(nom, dir)
            row.append(fil.date)

        if force or None in row: date = None # must rebuild
        elif row:
            date = min(row) # oldest output
            if date < self.date: date = None
        else: # no min; fake a non-None answer
            date = self.date
            assert None, ('how come this unwanted Table got asked for ?', names)

        return bok, date

    def load(self, fwd):
        """Loads sources.

        Required argument is a data structure, to be passed down to the
        callables self.source() received to handle self's sources.  This will
        normally be a mutable item-carrier (i.e. list or dictionary) in which
        the forward table, parsed from sources, will be stored. """

        s = self.__sources
        while s:
            (src, meth), s = s[0], s[1:]
            src = open(src, 'r')

            try: meth(src, fwd)
            finally: src.close()

        return fwd

    def writepairs(self, out, endian, all):
        """Emit a sorted sequence of pairs.

        Arguments:
          out -- tableFile to which to write data
          endian -- needed when opening that file
          all -- sequence of pairs (will be sort()ed)

        Writes each pair (k, v) using format 'H' for k and '<H' for v.
        """
        all.sort()
        out.open(endian)
        try:
            for k, v in all:
                out.emit('H', k)
                out.emit('<H', v)
        finally:
            out.close()

class textTable (Table):
    """Base-class for tables derived from line-oriented sources/*.txt files.

    Over-rides source(), providing a standard mechanism for reading *.txt source
    tables. """

    __upsource = Table.source
    def source(self, stem):
        """Announces the sources/*.txt file for this table.

        Single argument, stem, is the * in that filename; this method should be
        called by derived class constructors (after recursive call to this
        base-class' constructor) to declare which source file self's table is
        (or tables are) generated from.  That file will then be available to
        load(), q.v.  """
        self.__upsource(stem + '.txt', self.digest)

    comment_delimiter = '#' # over-ride in derived classes if needed
    def digest(self, src, fwd,
               chk=Checker, find=string.find, split=string.split):
        """Digest a single file (as a stream) of table source.

        Arguments (all required; supplied by .load()):
          src -- file handle from which to read data
          fwd -- arg passed to load (q.v.)

        Pseudo-tables which aren't really building a unicode mapping table
        should probably over-ride this method in its entirety; the base-class
        implementation is designed for real unicode tables. """

        cmnt = self.comment_delimiter
        if not cmnt: find = lambda l, c: -1 # suppress comment splitting
        chk = chk.instance

        for line in src:
            cut = find(line, cmnt)
            if cut < 0: comment = None
            else: line, comment = line[:cut], line[cut + len(cmnt):]

            try: byte, unicode = self.parse(split(line))
            except (ValueError, IndexError), what: pass
            else:
                fwd[byte] = unicode
                if chk and comment:
                    chk.notice(unicode, self.name, comment)

    parse_spec = \
        """Parses a (semi-digested) line from a source file.

        Single argument is a sequence holding the fragments into which the line
        (with any comment part removed, if appropriate) gets broken by
        string.split().  If the line doesn't contribute to the table, this
        method should raise ValueError or IndexError; these (and only these)
        will be caught by load().  Otherwise, it should return a pair (tuple of
        length 2); the first is the charset's input (archetypically, the
        byte-value for a single-byte encoding), the second its output (i.e. the
        unicode code-point). """

del os, string, Checker
# and tableFile isn't intended as an export, it's just needed as a global.
