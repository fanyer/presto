#!/usr/bin/env python

"""\
Strips ifdefs from a file based on what is defined.

This script can either be used as a module or directly using
it's main(). See -h for commandline usage.

The main function is removeIfDefs().

To protect certain #ifdef checks from removal you can use:

  #pragma keep_contitionals SAVE_ME

which would leave checks such as...

  #ifdef SAVE_ME
    Something...
  #endif // SAVE_ME

...alone. You can enter several whitespace separated conditionals to keep.

This script can also be used to numerate enum values. If you specify @VAL@ as
the value. For example:

 enum
 {
    A = @VAL@,
    B = @VAL@
 }

would result in:

 enum
 {
    A = 0,
    B = 1
 }

"""

# Builtin modules
import os.path
import os
import sys
import getopt
import copy
import re
import tempfile
import filecmp

# Modules in hardcore/scripts/.
import preprocess as pp
import util

def main(argv):
    """Testfiles are currently looked for in adjunct/mantle/tools/bin"""
    # The directory where this script is located
    script_dir = os.path.abspath(os.path.dirname(sys.argv[0]))
    # The top opera directory, found by assuming we know where we are
    opera_dir = os.path.normpath(os.path.join(script_dir,"../../../"))

    if not os.path.exists(opera_dir):
        print "The path '%s' does not exist.\n" % opera_dir

    os.chdir(opera_dir)

    # Predefined macros (dict)
    macros   = {}

    try:
        opts, args = getopt.getopt(argv, "s:o:f:d:wthl", ["help"])
    except getopt.GetoptError, e:
        print "ERROR - (%s) See -h for available options and usage\n" % e.msg
        sys.exit(2)

    opt_defines = []
    opt_files = []
    opt_system = 0
    opt_outdir = ""
    opt_warnings = 0
    opt_testing = 0
    opt_lines = 0

    for opt, arg in opts:
        if opt in ("-h", "--help"):
            usage()
            sys.exit()
        elif opt == '-d':
            opt_defines = arg.split()
        elif opt == '-f':
            opt_files = arg.split()
        elif opt == '-s':
            opt_system = arg
        elif opt == '-o':
            opt_outdir = arg
            if opt_outdir[len(opt_outdir)-1] != '/':
                opt_outdir += '/'
        elif opt == '-w':
            opt_warnings = 1
        elif opt == '-t':
            opt_testing = 1
        elif opt == '-l':
            opt_lines = 1

    if not opt_system:
        if opt_testing:
            # If we are testing just use the lingogi file
            opt_system = os.path.join('platforms', 'lingogi', 'system.h')
        else:
            err("You need to specify -s, see -h")
            sys.exit(2)

    if not os.path.exists(opt_system):
        err("'%s' does not exist" % opt_system )
        sys.exit(2)

    # First do all preprocessing from pch.h
    processor = pp.Preprocessor()
    processor.__setitem__("PRODUCT_SYSTEM_FILE", '"'+opt_system+'"')
    processor.addUserIncludePath(".")
    processor.ignoreErrors()

    # If defines are specified we parse them here
    # currently treated as one space separated string
    # and we replace \" with " in string defines.
    #
    # FIXME: The python macros store the name
    # and value, the current code is pike inherited
    # and thus redundantly stores the name twice
    #
    if opt_defines:
        for define in opt_defines:
            if len(define):
                split = define.split("=")
                if len(split) > 1:
                    macros[split[0]] = pp.Macro(split[0], None, split[1].replace("\\\"", "\""))
                else:
                    macros[define] = pp.Macro(define, None, "")
                processor.update(macros)

    #  Specify files to strip

    files_to_strip = []

    if len(opt_files):
        files_to_strip = opt_files
    else:
        gogi_include_dir = "platforms/gogi/include/"
        files_to_strip = [ gogi_include_dir+"gogi_global_history.h",
                           gogi_include_dir+"gogi_mem_api.h",
                           gogi_include_dir+"gogi_opera_api.h",
                           gogi_include_dir+"gogi_plugin_api.h",
                           gogi_include_dir+"gogi_plugin.h",
                           gogi_include_dir+"gogi_prefs.h" ]

    info("Collecting ifdefs using pch.h...")
    processor.preprocess("core/pch.h")

    if opt_testing:
        macros["CMD1"] = pp.Macro("CMD1", None, "")
        macros["CMD3"] = pp.Macro("CMD3", None, "")
        processor.update(macros)
        runTesting(macros, True, opt_lines, processor)
    else:
        for f in files_to_strip:
            header_path = os.path.basename(f)
            removeIfdefs(f, opt_outdir+header_path, macros, opt_warnings, opt_lines, processor)


def runTesting(macros, warnings, opt_lines, processor):
    """ Tests the script using fixed files with testcases.
    The word FAIL should never be visible in the output except for in comments.
    If FAILS are found we exit and print the number of fails.
    Manual inspection is needed at this point. """

    info("\n=== Performing selftests, one ERROR above this line is expected, FAIL should be 0 ===\n")

    infile1  = os.path.join('modules', 'hardcore', 'scripts', 'tests', 'strip_ifdefs_test.h')
    infile2  = os.path.join('modules', 'hardcore', 'scripts', 'tests', 'strip_ifdefs_test2.h')
    infile3  = os.path.join('modules', 'hardcore', 'scripts', 'tests', 'strip_ifdefs_enums.h')
    outfile1 = os.path.join('modules', 'hardcore', 'scripts', 'tests', 'strip_ifdefs_test_stripped.h')
    outfile2 = os.path.join('modules', 'hardcore', 'scripts', 'tests', 'strip_ifdefs_test_stripped2.h')
    outfile3 = os.path.join('modules', 'hardcore', 'scripts', 'tests', 'strip_ifdefs_enums_stripped.h')
    removeIfdefs(infile1, outfile1, macros, warnings, processor)
    removeIfdefs(infile2, outfile2, macros, warnings, processor)
    removeIfdefs(infile3, outfile3, macros, warnings, processor)

    try:
        fo1 = None
        try:
            fo1 = open(outfile1, 'rU')
            data1 = fo1.read()
        finally:
            if fo1: fo1.close()
        fo2 = None
        try:
            fo2 = open(outfile2, 'rU')
            data2 = fo2.read()
        finally:
            if fo2: fo2.close()
    except IOError, e:
        warn("Could not open outfile '%s' (%s)" % (e.filename, e.strerror) )
        return

    data1 = strip_comments(data1)
    data2 = strip_comments(data2)
    fails = data1.count("FAIL")
    fails += data2.count("FAIL")
    info("Number of FAIL's: %d" %fails)

    try:
        fi1 = None
        try:
            fi1 = open(infile1, 'rU')
            idata1 = fi1.read()
        finally:
            if fi1: fi1.close()
        fi2 = None
        try:
            fi2 = open(infile2, 'rU')
            idata2 = fi2.read()
        finally:
            if fi2: fi2.close()
    except IOError, e:
        warn("Could not open infile '%s' (%s)" % (e.filename, e.strerror) )
        return

    ioks = idata1.count("OK")
    ioks += idata2.count("OK")
    ooks = data1.count("OK")
    ooks += data2.count("OK")
    if ioks != ooks:
        info("Number of Input OK's and Output OK's differ: %d/%d" % (ioks, ooks))

    enum_reference = os.path.join('modules', 'hardcore', 'scripts', 'tests', 'strip_ifdefs_enums_ref.h')
    if not filecmp.cmp(outfile3, enum_reference):
        info("ERROR - The enum output and reference do not match (%s != %s)" % (outfile3, enum_reference) )
        sys.exit(1)

    if not fails and (ioks==ooks):
        sys.exit(0)
    else:
        sys.exit(1)


def is_ws(c):
    """Return whether a character is whitespace
    This could be replaced with a regexp but currently
    duplicate the previous pike script exactly"""
    return (c == ' ' or c == '\t' or c == '\n')

class StringTok:
    """Convenience class for easy retrieval of next token in a line
    Tokens are separated by whitespaces."""

    def __init__(self, str_):
        self.str = str_
        self.ls = len(self.str)
        self.i = 1

    def next_token(self):
        """ Returns next token as a string or '' if there is no more """
        while( self.i < self.ls and is_ws(self.str[self.i]) ):
            self.i += 1
        start = self.i
        while( self.i < self.ls and not is_ws(self.str[self.i]) ):
            self.i += 1
        return self.str[start:self.i]

    def rest_of_line(self):
        """ Returns the current remains of the input string """
        ret = self.str[self.i+1:self.ls]
        self.end()
        return ret

    def end(self):
        """After this call calls to next_token will return '' """
        self.i = self.ls


def removeIfdefs(infile, outfile, predefined_macros, syntax_warn, line_numbers, processor):
    """\
    Parses one file and strips away all undefined lines

    @param infile  The input template file to be stripped
    @param outfile The output stripped filename
    @param predefined_macros dict (string->Macro), can be None
    @param syntax_warn if True errors will be treated as warnings
    @param line_numbers if True, #line directives will be generated pointing to the template header
    @param processor The processor object on which preprocess()
                     has already been called.

    """

    #info("'%s' -> '%s'" % (os.path.basename(infile),
    #                       os.path.basename(outfile)))

    if infile == outfile:
        err("infile and outfile are the same. ('%s')" % infile)
        return False

    if not util.fileTracker.addInput(infile):
        warn("Could not locate file '%s' (cwd=%s)" %(infile, os.getcwd()))
        return False

    try:
        f = None
        try:
            f = open(infile, 'rU')
            data = f.read()
        finally:
            if f: f.close()

        if os.path.exists(outfile):
            of = open(outfile,'rU')
        else:
            of = None
        tf = tempfile.TemporaryFile()
    except IOError, e:
        warn("Could not open file '%s' (%s)" % (e.filename, e.strerror) )
        return False

    # Copy the predefined macros such that we do not modify them
    if predefined_macros != None:
        macros = copy.copy(predefined_macros)
    else:
        macros = {}

    def has_macro(mak):
        """ Return True if the macro is defined locally or globally """
        return macros.has_key(mak) or processor.__contains__(mak)

    # This mapping tells us what defines to leave as is
    keep_conditionals = {}

    # Unmodified lines stored for later retrieval
    olines = data.split('\n')

    # Start with stripping all comments
    # such that they do not interfere with the rest
    data = strip_comments(data)

    # Non comment lines
    lines = data.split('\n')

    skip = 0
    branch_taken = [] # Used as a stack
    lines_to_use = []
    lc = 0 # Line count
    enum_counter = 0 # Increasing counter for enum values
    enum_warn = False

    for line in lines:
        line_t = StringTok(line)
        if line_t.ls:
            # Check directives
            if line[0] == '#':
                #print "Reading %s (%d)" % (line, line_t.ls)
                # Finds current directive
                directive = line_t.next_token()
                # 'if(n)def' creates branch
                if directive == "ifdef" or directive == "ifndef":
                    ifdef = directive == "ifdef"
                    d = line_t.next_token()
                    if not skip and keep_conditionals.has_key(d):
                        lines_to_use += [lc]
                        branch_taken.append(2) # 2 marks that we are inside a keep_conditionals define
                    else:
                        if skip or (ifdef and not has_macro(d)) or (not ifdef and has_macro(d)):
                            skip += 1
                        if skip == 0:
                            branch_taken.append(1)
                        else:
                            branch_taken.append(0)

                # 'else' enters branch if not entered before
                elif directive == "else":
                    if branch_taken[-1] == 2:
                        lines_to_use += [lc]
                    elif not branch_taken[-1]:
                        if skip == 1:
                            skip = 0
                            branch_taken.pop()
                            branch_taken.append(1)
                        # else we just continue to skip at the same depth
                    else:
                        skip = 1

                elif directive == "endif":
                    if len(branch_taken) < 1:
                        err("Exit of unentered branch. (Line:%d)" % (lc+1))
                        sys.exit(1)
                    if branch_taken[-1] == 2:
                        lines_to_use += [lc]
                    branch_taken.pop()
                    if skip:
                        skip -= 1

                elif directive == "if" or directive == "elif":
                    _elif = directive == "elif"
                    expr = line_t.rest_of_line()
                    # value will be evaluated to true or false
                    value = processor.evaluate(infile, lc+1, expr)
                    if _elif:
                        if skip == 1 and not branch_taken[-1]:
                            skip = not value
                            if not skip:
                                branch_taken.pop()
                                branch_taken.append(1)
                        elif not skip:
                            skip = 1
                    else:
                        if skip:
                            skip += 1
                        else:
                            skip = not value
                        if skip == 0:
                            branch_taken.append(1)
                        else:
                            branch_taken.append(0)

                elif directive == "pragma":
                    pragma = line_t.next_token()
                    if pragma == "keep_conditionals":
                        leave = line_t.next_token()
                        while leave != "":
                            keep_conditionals[leave] = 1
                            leave = line_t.next_token()
                    elif not skip:
                        lines_to_use += [lc]
                    line_t.end()

                elif directive == "define":
                    d = line_t.next_token()
                    d2 = line_t.rest_of_line()
                    macros[d] = pp.Macro(d, None, d2)
                    if not skip:
                        lines_to_use += [lc]

                else:
                    line_t.end()
                    if not skip:
                        lines_to_use += [lc]

                more = line_t.next_token()
                if more != "":
                    err("%s:%d uses unhandled syntax! (%s)" % (infile, lc+1, more) )
                    if not syntax_warn:
                        sys.exit(1)

            else:
                if not skip:
                    lines_to_use += [lc]

                # Number the enums
                # Every enum value that is specified to @VAL@
                # will be set to the current counter.
                # If an enum contains a fixed value the internal
                # counter will be updated to it.
                ENUM_VALUE = re.compile(r'(.*\w+\s*=\s*)([^,]*)(.*)')
                ENUM_DEF   = re.compile(r'\s*(typedef\s+)?enum(\s+\w+)?(\s*{)?\s*$')

                if re.match(ENUM_DEF, line):
                    enum_counter = 0
                    enum_warn = False
                else:
                    if re.match(ENUM_VALUE, line):
                        m = re.match(ENUM_VALUE, olines[lc])
                        if m.group(2) == "@VAL@":
                            olines[lc] = "%s%d%s" % (m.group(1), enum_counter, m.group(3))
                            enum_counter += 1
                            if enum_warn:
                                warn("Setting enum value in enum with unparsable value(s) (%s)" % enum_warn)
                                enum_warn = False
                        else:
                            try:
                                enum_counter = int(m.group(2), 0)+1
                            except ValueError:
                                enum_warn = m.group(2)

        else:
            if not skip:
                lines_to_use += [lc]

        lc += 1

    # Will write to a temp file and compare
    # to existing file. A bit wasteful
    # would be better if the make system could
    # check if the template had been modified
    # before running this script at all.
    next_line = 0
    for line in lines_to_use:
        if line_numbers and next_line != line:
            # FIXME Needs to escape the filename
            tf.write("#line %d \"%s\"\n" % (line + 1, infile));
        next_line = line + 1
        tf.write("%s\n" % olines[line])
    tf.flush()

    changes = True
    os.lseek(tf.fileno(), 0, 0)
    tf_data = tf.read()

    if of:
        of_data = of.read()
        if tf_data == of_data:
            info("No changes to %s" % os.path.basename(outfile))
            of.close()
            changes = False

    if changes:
        if of:
            of.close()
        try:
            of = open(outfile, 'w')
        except IOError, e:
            warn("Could not open file '%s' (%s)" % (e.filename, e.strerror) )
            return False

        os.write(of.fileno(), tf_data)
        info("Wrote %s" % os.path.basename(outfile))
        of.close()

    tf.close()

    return True

def strip_comments(data):
    """ Replace all commented out parts by empty space """
    # This regexp matches either a block comment or line comment
    COMMENT = re.compile(r"/\*.*?\*/|//.*?$", re.DOTALL|re.MULTILINE)
    # We should keep the newlines so all except them will be replaced
    NON_NL = re.compile(r"[^\n]")
    def replace(m):
        """ For every match we want to ' '-fill it """
        return re.sub(NON_NL, ' ', m.group(0))
    return re.sub(COMMENT, replace, data)

def info(information):
    """ Info messages """
    print "%s" % information

def err(information):
    """ Error messages """
    print "Ifdef-stripping: ERROR: %s" % information

def warn(information):
    """ Warning messages """
    print "Ifdef-stripping: WARNING: %s" % information

def usage():
    """Print commandline usage information"""
    print """
Usage: strip_ifdefs.py -s PRODUCT_SYSTEM_FILE -o OUTDIR [-h] [-w] [-l] [-t] [-d defines] [-f files]

Running this script will collect all defines set from inclusion of pch.h and the
system file specified using -s. Then it will strip away ifdef checks from all
gogi headers in gogi/include (or files specified when using -f) and put the stripped
results in a specified destination.

To protect certain #ifdef checks you can use:  #pragma keep_contitionals SAVE_ME
which would leave checks such as:

  #ifdef SAVE_ME
    Something...
  #endif // SAVE_ME

left alone. You can enter several whitespace separated conditionals to keep.

This script can also be used numerate enum values. If you specify @VAL@ as
the value. For example:

 enum
 {
    A = @VAL@,
    B = @VAL@
 }

would result in:

 enum
 {
    A = 0,
    B = 1
 }

Options:
 -s PRODUCT_SYSTEM_FILE        The system file for your product,
                               with path relative top level opera dir.
 -o OUTDIR                     The directory where to put all stripped headers,
                               with path relative top level opera dir.
 -f files                      Must be last argument, and performs stripping of
                               mentioned files instead of the default gogi headers.
 -d defines                    Space separated list of preset defines.
                               Remember to quote this list if there is more than one define.
 -w                            Turns syntax errors into warnings.
 -l                            Adds #line directives in the stripped files
 -t                            Perform testing by stripping strip_ifdefs_test.h
 -h                            This help
"""

if __name__ == "__main__":
    main(sys.argv[1:])
