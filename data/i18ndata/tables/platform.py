"""Platform-description specifics
"""

# Copyright Opera software ASA, 2003-2004.  Written by Eddy.
import string, os
from tables import table, known
from tablefile import describe

class Platform:
    """Controlling object for chartable construction.

    The object constructed is responsible for parsing the tables file
    and constructing the chartable it specifies.  See constructor for
    further details.\n"""

    def __init__(self, log, err):
        """Construct platform-descriptor object

        Takes two arguments, stdout and stderr file descriptors, for
        use in logging and error reporting.  The object's .parse(file)
        must subsequently be fed a table file before its .ready() will
        return true; after which a call to .generate (q.v.) will build
        the chartables file.\n"""

        self.version = None
        self.emojis = []
        self.log, self.err = log, err

    def parse(self, file,
              strip=string.strip, find=string.find):
        """Parses a table-list file.

        Single argument is the name of the file to be parsed.\n"""

        bok, n = {}, 0
        fd = open(file) # let caller catch any IOError

        try:
            for line in fd:
                n = 1 + n
                if '#' in line: line = line[:find(line, '#')]
                line = strip(line)
                if not line: pass
                elif line[0] != '@': bok[line] = None
                elif line[1:9] == 'version ': self.version = int(line[9:])
                elif line[1:8] == 'emojis ':
                    self.emojis.extend(map(str.strip, line[8:].split('+')))
                # Deprecated @imode
                elif line[1:] == 'imode':
                    self.emojis.append("imode")
                    self.err.write('@imode is deprecated. Use "@emojis imode"'
                                   ' instead, %s:%d\n' % (file, n))
                else:
                    self.err.write('Ignoring unrecognised @-line "%s", %s:%d\n'
                                   % (line, file, n))
        finally:
            fd.close()

        if self.version is None:
            raise ValueError('No @version line')
        elif self.version not in (6, 7, 8):
            raise ValueError('Unsupported @version, %d,' % self.version)

        if file[:7] == 'tables-': file = file[7:]
        if file[-4:] == '.txt': file = file[:-4]
        self.__plat, self.tables = file, bok

    def ready(self):
        """Returns true if ready to generate tables.

        This just tells you whether self.parse() has succeeded.  See
        the logic of main()'s argument-parsing for why that's needed
        ...\n"""

        try: self.__plat
        except AttributeError: return None
        return 1

    def generate(self, filename, endian, force=None, verbose=0, tempdir=None,
                 # transcribe imports so we can del them below
                 table=table, known=known, describe=describe,
                 mkdir=os.mkdir, exists=os.path.isdir, join=string.join):
        """Generates the chartables file.

        Required arguments:
          filename -- name for output file; if empty (or None) a default is
                      used.
          endian -- either '<' for little-endian or '>' for big-endian

        Optional arguments:
          force -- if true, all tables are regenerated; otherwise (the default),
                   only those whose sources have changed are.
          verbose -- verbosity level; none for quiet, 1 for hash-mode, 2 for
                     verbose.
        """

        if tempdir:
            assert filename, 'You must specify a non-empty filename when specifying tempdir'
            dirname = tempdir
        else:
            if self.emojis: dirname = "+".join(sorted(set(self.emojis)))
            else: dirname = 'plain'
            if endian == '>':
                dirname = dirname + '-be'
                if not filename: filename = 'chartables-be.bin'
            else:
                assert endian == '<', 'Bad value for endianness flag'
                dirname = dirname + '-le'
                if not filename: filename = 'chartables.bin'

        if not exists(dirname): mkdir(dirname)
        if verbose > 1: old = []
        bok = {}

        # Build all the tables:
        for key in self.tables.keys():
            if bok.has_key(key): continue # handled by an earlier table
            tbl = table(key, self.emojis) # pass in version as well ?
            if tbl.name == 'charsets': # special case ...
                tbl.enlist(known(), self.tables.has_key, self.version)

            if verbose > 1:
                self.log.write('Generating %s ...' % key) # no dangling '\n'

            bok.update(tbl.build(endian, dirname, force, self.tables.has_key))

            assert bok.has_key(key), \
                   ('mismatch between tables named and created',
                    key, tbl.name, bok.keys())

            if verbose > 1:
                new = filter(lambda x, o=old: x not in o, bok.keys())
                old = old + new
                assert new
                new.sort()
                if len(new) == 1: self.log.write(' generated %s\n' % new[0])
                else: self.log.write(' generated %s and %s\n' %
                                     (join(new[:-1], ', '), new[-1]))

            elif verbose:
                self.log.write('#')
                self.log.flush()

        del self.tables # free up some memory
        if verbose > 1: self.log.write('Done.\n')
        elif verbose: self.log.write('\n')

        row = bok.items()
        # each entry is (charset-name, tableFile())
        del bok # free up some more memory
        row.sort() # by charset name

        # Emit to target file:
        try: target = open(filename, 'wb')
        except IOError:
            self.err.write('Failed to open %s\n' % filename)
            return 1

        try: describe(row, endian, target)
        finally: target.close()

del string, os, table, known, describe # i.e. everything but Platform
