"""Chartable generator.

This file contain the superstructure, without knowledge of intimate details of
charsets, tables, etc.  See:

 * platform.py for the Platform class, which drives the process.
 * tables.py for the junction box associating table names with classes; see its
   import statements for which classes come from which files.
 * basetable.py for the base-classes for individual table handlers
 * tableutils.py for some utilities used by many table handlers

The primary export of this file is the function main(), q.v., and the
command-line it parses is described in increasing detail by functions usage(),
summary() and help(), below.
"""

# Copyright Opera software ASA, 2003.  Written by Eddy.
from platform import Platform

def usage(tgt, myname):
    tgt.write('Usage: %s [ -u | -h | --help] [options] table-###.txt\n' % myname)

def summary(tgt, myname):
    usage(tgt, myname)
    tgt.write("""
Prepares the character tables needed by Opera's encodings module.
Option summary:
 -a or --makeall		Rebuild all tables, even up-to-date ones
 -b or --big-endian		Build tables for big-endian
 -c or --check-sources		Sanity-check sources (experimental)
       --hash-mode              Produces # indicators of progress
 -l or --little-endian		Build tables for little-endian
 -n or --native-endian		Build native tables (default)
 -o name or --output-file name	Specify where to write results
 -t dir or --tempdir dir	Directory for temporary files
 -v or --verbose                Mention each table as it is processed
 -u or --usage	Produce terse usage message and exit
 -h		Produce short usage summary with options and exit
       --help	Produce long-winded help and exit (hint: ... | more)
""")

def help(tgt, myname):
    summary(tgt, myname)
    tgt.write("""
The tables-*.txt files in this directory are plain text files listing
the tables needed for different builds of Opera.  Select one and supply
it on the command-line to control which tables will be built.

The generated tables are stored in a binary format; big- and little-
endian binaries for Opera require different versions of the output
tables file.  Use the -n, -b, -l (that's `dash ell', *not* `minus one')
flags to build native, big-endian or little-endian versions of the file.
You can only build one in any single run of this program; the last one
seen on the command-line takes precedence.  The native version
auto-detects which of the other two to use; it is the default.

Normally, a component table will only be rebuilt if files on which it
depends have been modified since it was built.  You can over-ride this
with the -a flag, forcing all component tables to be rebuilt.  The final
output file, packaging the various tables into one file, is rebuilt in
any case.  You can specify its name using the -o flag; otherwise the
output file's name is chartables.bin for little-endian or
chartables-be.bin for big-endian.  This is the name Opera expects to
find the file given in /usr/share/opera/ or its equivalent.

By default (though the make.py interface supplies -v by default, to
approximate backward compatibility) no output is produced unless errors
or surprises are encountered.  If --hash-mode or --verbose is specified,
whichever of them appears last on the command-line takes precedence; in
hash-mode, one '#' character is output for each table-generation
performed (even if it generates several tables); in verbose mode, a line
of output is produced, for each table-generation, indicating which
tables were generated.

The -c flag enables some rather minimal (and not necessarily
trustworthy) checking of the source data for consistency.
This is experimental.
""")

def main(self, args, out, err):
    """Command-line parser, front-end for chartable generation.

    Takes four arguments: the name under which the program is invoked
    (canonically sys.argv[0]), the list of arguments it was passed
    (sys.argv[1:]) and the file-handles for standard output (sys.stdout) and
    error (sys.stderr).\n"""

    makeall = check = outfile = grab = None
    endian, platform, verbose = '=', Platform(out, err), 0
    tempdir = None

    for arg in args:
        if grab == 'outfile': outfile, grab = arg, None
        elif grab == 'tempdir': tempdir, grab = arg, None

        elif arg in ( '-o', '--output', '--output-file' ): grab = 'outfile'
        elif arg in ( '-t', '--tempdir' ): grab = 'tempdir'

        elif arg in ( '-a', '--makeall' ): makeall = 1
        elif arg in ( '-b', '--big', '--big-endian' ): endian = '>'
        elif arg in ( '-l', '--little', '--little-endian' ): endian = '<'
        elif arg in ( '-n', '--native', '--native-endian' ): endian = '='
        elif arg in ( '-v', '--verbose' ): verbose = 2
        elif arg in ( '--hash', '--hash-mode' ): verbose = 1

        elif arg in ( '-u', '--usage' ): return usage(out, self) or 0
        elif arg == '-h': return summary(out, self) or 0
        elif arg == '--help': return help(out, self) or 0

        elif arg in ( '-c', '--check', '--check-sources' ):
            if not check:
                from checker import Checker
                check = Checker()

        else:
            try: platform.parse(arg)
            except ValueError, e:
                err.write('%s in %s\n' % (e.args[0], arg))
                return 1
            except IOError, e:
                err.write('Error reading %s' % arg)
                return 1

    if not platform.ready():
        usage(err, self)
        return 1

    if endian == '=':
        import struct
        if struct.pack('<H', 1) == struct.pack('=H', 1): endian = '<'
        else: endian = '>'
        del struct

    ans = platform.generate(outfile, endian, makeall, verbose, tempdir)
    if check:
        bad = check.check(err)
        if bad:
            err.write('Found %d conflicts.\n' % bad)
            if not ans: ans = 2
    return ans

if __name__ == '__main__':
    import sys
    try: ans = main('python %s' % sys.argv[0], sys.argv[1:], sys.stdout, sys.stderr)
    except KeyboardInterrupt:
        sys.stderr.write('You interrupted me.  I guess you know best.\n')
        ans = 0
    except:
        sys.stderr.write("Something went horribly wrong !\n")
        ans = -1

    sys.exit(ans)
