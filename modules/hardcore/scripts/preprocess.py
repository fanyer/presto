import re
import clexer
import itertools
import os
import os.path
import platform
import stat
import sys

import util

class Macro:
    def __init__(self, name, parameters, value):
        self.__name = name
        self.__parameters = parameters
        self.__value = value

    def name(self): return self.__name
    def parameters(self): return self.__parameters
    def value(self): return self.__value
    def tokens(self):
        if self.__value: return clexer.tokenize(clexer.split(self.__value, include_ws=False, include_comments=False))
        else: return []

    def __cmp__(self, other): return cmp((self.__name, self.__value), (other.__name, other.__value))

class Macros:
    def __init__(self, base, undefined=None):
        self.__local = {}
        if undefined is not None: self.__local[undefined] = None
        self.__base = base

    def __getitem__(self, name):
        if name in self.__local:
            macro = self.__local[name]
            if macro is None: return KeyError
            else: return macro
        else:
            return self.__base[name]

    def __setitem__(self, name, macro):
        self.__local[name] = macro

    def __contains__(self, name):
        if name in self.__local: return self.__local[name] is not None
        else: return name in self.__base

class Preprocessor:
    RE_PPDIRECTIVES = re.compile("^\\s*(#)\\s*((?:\\\\\n|[^\n])*?)(?:$|//[^\n]*$|/\\*.*?(?:\n.*?\\*/|.*?\\*/\\s*$))|//.*?\n|/\\*.*?\\*/", re.MULTILINE | re.DOTALL)
    RE_SPACE = re.compile("\\s+")

    def __init__(self):
        self.__macros = {}
        self.__initialMacros = {}
        self.__quiet = False
        self.__verbose = False
        self.__ignoreErrors = False
        self.__userIncludePaths = []
        self.__systemIncludePaths = []
        self.__searchSameDirectory = False
        self.__debug = False
        self.__userFiles = set()
        self.__systemFiles = set()
        self.__failed = False

    def __getitem__(self, name):
        return self.__macros.get(name)

    def __setitem__(self, name, value):
        macro = Macro(name, None, value)
        self.__initialMacros[name] = macro
        self.define(Macro(name, None, value))

    def __contains__(self, name):
        return name in self.__macros

    def macros(self): return self.__macros.values()

    def update(self, macros):
        self.__macros.update(macros)

    def addUserIncludePath(self, path):
        self.__userIncludePaths.append(path)

    def addUserIncludePaths(self, paths):
        self.__userIncludePaths.extend(paths)

    def removeUserIncludePath(self, path):
        self.__userIncludePaths.remove(path)

    def searchSameDirectory(self):
        self.__searchSameDirectory = True

    def addSystemIncludePath(self, path):
        self.__systemIncludePaths.append(path)

    def setSystemIncludePaths(self, paths):
        self.__systemIncludePaths = paths[:]

    def ignoreErrors(self):
        self.__ignoreErrors = True

    def setQuiet(self):
        self.__quiet = True

    def setVerbose(self):
        self.__verbose = True

    def userFiles(self):
        return list(self.__userFiles)

    def systemFiles(self):
        return list(self.__systemFiles)

    def reset(self):
        self.__macros = dict(self.__initialMacros)
        self.__userFiles = set()
        self.__systemFiles = set()

    def warning(self, filename, linenr, message):
        if not self.__quiet:
            if "WINGOGI" in self.__macros or platform.system() in ("Windows", "Microsoft"):
                print >>sys.stderr, "%s(%d) : warning W0000: %s" % (filename, linenr, message)
            else:
                print >>sys.stderr, "%s:%d: warning: %s" % (filename, linenr, message)

    def error(self, filename, linenr, message):
        if "WINGOGI" in self.__macros or platform.system() in ("Windows", "Microsoft"):
            print >>sys.stderr, "%s(%d) : error C0000: %s" % (filename, linenr, message)
        else:
            print >>sys.stderr, "%s:%d: error: %s" % (filename, linenr, message)
        self.__failed = True

    def define(self, macro):
        if self.__debug:
            print >>sys.stderr, "defined %s => %r" % (macro.name(), macro.value())
        self.__macros[macro.name()] = macro

    def undef(self, name):
        del self.__macros[name]

    class SkipFile(Exception): pass

    def enter(self, directive, path, fullPath): pass
    def leave(self, directive, path, fullPath): pass

    def preprocess(self, filename, source=None):
        """Read 'source', which is a filename or a file-like object, and process all
preprocessor directives in it."""
        if source is None:
            f = None
            try:
                f = open(filename)
                util.fileTracker.addInput(filename)
                source = f.read()
            finally:
                if f: f.close()

        def preprocessInner(self, filename, source):
            class Conditional:
                def __init__(self, include, canTakeBranch=None):
                    self.__include = include
                    if canTakeBranch is None: self.__canTakeBranch = not include
                    else: self.__canTakeBranch = canTakeBranch
                def __nonzero__(self): return self.__include
                def canTakeBranch(self): return self.__canTakeBranch
                def takeBranch(self):
                    self.__include = True
                    self.__canTakeBranch = False
                def endBranch(self):
                    self.__include = False
            
            conditional = []

            for match in Preprocessor.RE_PPDIRECTIVES.finditer(source):
                def linenr():
                    region = source[:match.start()]
                    return region.count("\n") + 1

                if not match.group(1): continue

                line = match.group(2)
                parts = Preprocessor.RE_SPACE.split(line, 1)

                if len(parts) >= 1:
                    directive = parts[0]
                    if len(parts) >= 2: data = parts[1]
                    else: data = ""
                else:
                    continue

                if conditional and not conditional[-1]:
                    if directive in ("if", "ifdef", "ifndef"):
                        conditional.append(Conditional(False, False))

                    elif directive == "else":
                        if conditional[-1].canTakeBranch(): conditional[-1].takeBranch()

                    elif directive == "elif":
                        if conditional[-1].canTakeBranch():
                            if self.evaluate(filename, linenr, data):
                                conditional[-1].takeBranch()

                    elif directive == "endif":
                        conditional.pop()

                elif directive == "define":
                    tokens = list(clexer.tokenize(clexer.split(data, include_comments=False)))

                    while tokens and tokens[0].isspace(): tokens.pop(0)

                    if tokens[0].isidentifier():
                        name = str(tokens[0])
                        tokens.pop(0)

                        parameters = None
                        value = None

                        if tokens and tokens[0] == "(":
                            parameters = []
                            tokens.pop(0)
                            if not tokens:
                                self.error(filename, linenr(), "ignoring malformed #define directive")
                                continue
                            elif tokens[0] != ")":
                                while True:
                                    if not tokens:
                                        self.error(filename, linenr(), "ignoring malformed #define directive")
                                        break
                                    elif not tokens[0]:
                                        tokens.pop(0)
                                        continue
                                    elif tokens[0].isidentifier(): parameters.append(str(tokens[0]))
                                    else: self.warning(filename, linenr(), "unexpected token '%r' in macro parameter list" % str(tokens[1]))
                                    tokens.pop(0)
                                    while tokens and not tokens[0]: tokens.pop(0)
                                    if not tokens:
                                        self.error(filename, linenr(), "ignoring malformed #define directive")
                                        break
                                    elif tokens[0] == ")":
                                        break
                                    elif tokens[0] != ",":
                                        self.warning(filename, linenr(), "unexpected token '%r' in macro parameter list" % str(tokens[1]))
                                    tokens.pop(0)
                            tokens.pop(0)

                        while tokens and not tokens[0]: tokens.pop(0)
                        while tokens and not tokens[-1]: tokens.pop()

                        if tokens: value = clexer.join(tokens, insertSpaces=False)

                        self.define(Macro(name, parameters, value))
                    else:
                        self.error(filename, linenr(), "ignoring malformed #define directive")

                elif directive == "undef":
                    name = data.strip()
                    if name in self.__macros: del self.__macros[name]

                elif directive == "ifdef":
                    conditional.append(Conditional(data.strip() in self.__macros))

                elif directive == "ifndef":
                    conditional.append(Conditional(data.strip() not in self.__macros))

                elif directive == "if":
                    conditional.append(Conditional(self.evaluate(filename, linenr, data)))

                elif directive == "include":
                    originalData = data
                    data = data.strip()

                    if (not (data[0] == "\"" and data[-1] == "\"")
                        and not (data[0] == "<" and data[-1] == ">")):
                        tokens = clexer.tokenize(clexer.split(data, include_ws=False, include_comments=False))
                        data = "".join(map(str, self.expand(filename, linenr, tokens)))

                        if self.__debug:
                            print >>sys.stderr, "macro include %r => %r" % (originalData, data)

                    if data[0] == "\"" and data[-1] == "\"":
                        name = data[1:-1]
                        search = self.__userIncludePaths + self.__systemIncludePaths
                        if self.__searchSameDirectory: search.insert(0, os.path.dirname(filename))
                    elif data[0] == "<" and data[-1] == ">":
                        name = data[1:-1]
                        search = self.__systemIncludePaths
                    else:
                        name = None
                        self.error(filename, linenr(), "malformed #include directive ignored")

                    if name is not None:
                        for index, path in enumerate(search):
                            final = os.path.join(path, *name.split("/"))
                            if os.path.isfile(final):
                                try:
                                    self.enter("#include %s" % originalData, name, final)
                                    if path in self.__userIncludePaths: self.__userFiles.add(final)
                                    elif path in self.__systemIncludePaths: self.__systemFiles.add(final)
                                    f = None
                                    try:
                                        f = open(final)
                                        util.fileTracker.addInput(final)
                                        final_text = f.read()
                                    finally:
                                        if f: f.close()
                                    preprocessInner(self, final, final_text)
                                    self.leave("#include %s" % originalData, name, final)
                                except Preprocessor.SkipFile:
                                    pass
                                break
                        else:
                            if not self.__ignoreErrors:
                                self.error(filename, linenr(), "file %s not found; #include directive ignored" % data)

                elif directive == "elif":
                    if conditional: conditional[-1].endBranch()
                    else: self.error(filename, linenr(), "unexpected #elif directive ignored")

                elif directive == "else":
                    if conditional: conditional[-1].endBranch()
                    else: self.error(filename, linenr(), "unexpected #else directive ignored")

                elif directive == "endif":
                    if conditional: conditional.pop()
                    else: self.error(filename, linenr(), "unexpected #endif directive ignored")

                elif directive == "error":
                    if not self.__ignoreErrors:
                        self.error(filename, linenr(), "#error: %s" % data)

                else:
                    self.warning(filename, linenr(), "ignoring unhandled directive #%s" % directive)

        self.__userFiles.add(filename)

        preprocessInner(self, filename, source)
        return not self.__failed

    def expand(self, filename, linenr, tokens):
        def replaceArguments(tokens, arguments):
            tokens = iter(tokens)
            result = []

            while True:
                try: token = tokens.next()
                except StopIteration: break

                if token.isidentifier() and token in arguments:
                    if result and result[-1] == "#":
                        result[-1] = "\"%s\"" % clexer.join(filter(None, arguments[token])).replace("\\", "\\\\").replace("\"", "\\\"")
                    else:
                        result.append(arguments[token])
                else:
                    result.append(token)

            return result

        def processConcatenate(tokens):
            result = []
            for index, token in enumerate(tokens):
                if token == "##" and index not in (0, len(tokens) - 1):
                    left = result[0]
                    if isinstance(tokens[index + 1], list):
                        result.append(left + tokens[index + 1][0])
                        result.extend(tokens[index + 1][1:])
                    else:
                        result.append(left + tokens[index + 1])
                else:
                    result.append(token)
            return result
            
        def expandInner(self, tokens, expanding, macros):
            result = []

            while True:
                try: token = tokens.next()
                except StopIteration: break

                # "flatten" if we encounter a grouping we don't care about
                if isinstance(token, list):
                    tokens = itertools.chain(token, tokens)
                    continue

                if token.isidentifier() and token in self.__macros:
                    macro = self.__macros[token]

                    if macro.parameters():
                        try: arguments_lparen = tokens.next()
                        except StopIteration: break

                        if arguments_lparen != '(':
                            result.append(token)
                            result.append(arguments_lparen)
                            continue

                        arguments = clexer.partition(clexer.group1(tokens, ')'), ',')

                        if len(macro.parameters()) != len(arguments):
                            if not expanding or expanding is macro: context = ""
                            else: context = "while expanding macro '%s': " % expanding.name()
                            self.error(filename, linenr(), "%smacro '%s' requires %d arguments, %d given" % (context, macro.name(), len(macro.parameters()), len(arguments)))
                            return None

                        arguments = dict(zip(macro.parameters(), [expandInner(self, clexer.flatten(argument), expanding=macro, macros=macros) for argument in arguments]))

                        result.extend(expandInner(self, tokens=iter(processConcatenate(replaceArguments(macro.tokens(), arguments))), expanding=macro, macros=Macros(macros, macro.name())))
                    else:
                        result.extend(expandInner(self, tokens=iter(processConcatenate(macro.tokens())), expanding=macro, macros=Macros(macros, macro.name())))

                else:
                    result.append(token)

            return result

        return expandInner(self, iter(tokens), expanding=None, macros=Macros(self.__macros))

    def evaluate(self, filename, linenr, expression):
        tokens = clexer.tokenize(clexer.split(expression, include_ws=False, include_comments=False))

        intermediate = []
        while True:
            try: token = tokens.next()
            except StopIteration: break

            if token == "defined":
                try:
                    token = tokens.next()
                    if token == "(":
                        try:
                            name = tokens.next()
                            if tokens.next() != ")":
                                self.error(filename, linenr(), "invalid use of 'defined' in expression; evaluating to false")
                                return False
                        except StopIteration:
                            self.error(filename, linenr(), "invalid use of 'defined' in expression; evaluating to false")
                            return False
                    else: name = token
                    if name.isidentifier():
                        if name in self.__macros: intermediate.append(clexer.Token("1", 0, 0))
                        else: intermediate.append(clexer.Token("0", 0, 0))
                    else:
                        self.error(filename, linenr(), "invalid use of 'defined' in expression; evaluating to false")
                        return False
                    continue
                except StopIteration: pass
            else:
                intermediate.append(token)

        pythonized = []
        for token in self.expand(filename, linenr, intermediate):
            if token == "||": pythonized.append("or")
            elif token == "&&": pythonized.append("and")
            elif token == "!": pythonized.append("not")
            elif token.isidentifier(): pythonized.append("0")
            else: pythonized.append(str(token))

        if "?" in pythonized:
            def fixTertiary(tokens):
                if isinstance(tokens, list):
                    if "?" in tokens:
                        cond = tokens[1:tokens.index("?")]
                        ifyes = tokens[tokens.index("?") + 1:tokens.index(":")]
                        ifno = tokens[tokens.index(":") + 1:-1]
                        result = "[(%s), (%s)][int(bool(%s))]" % (fixTertiary(ifno), fixTertiary(ifyes), fixTertiary(cond))
                    else:
                        result = "(%s)" % " ".join(map(fixTertiary, tokens))
                    return result
                else:
                    return tokens
            pythonized = fixTertiary(["("] + clexer.group(pythonized) + [")"])
        else:
            pythonized = clexer.join(pythonized)

        try:
            return bool(eval(pythonized, { "int": int, "bool": bool }, {}))
        except:
            self.error(filename, linenr(), "failed to evaluate expression '%s' pythonized into '%s'; assuming false value" % (expression, pythonized))
            return False

    def filterTokens(self, tokens):
        class Conditional:
            def __init__(self, include, canTakeBranch=None):
                self.__include = include
                if canTakeBranch is None: self.__canTakeBranch = not include
                else: self.__canTakeBranch = canTakeBranch
            def __nonzero__(self): return self.__include
            def canTakeBranch(self): return self.__canTakeBranch
            def takeBranch(self):
                self.__include = True
                self.__canTakeBranch = False
            def endBranch(self):
                self.__include = False

        conditional = []

        for token in tokens:
            if token.isppdirective():
                match = re.match("^\\s*#\\s*([a-z]+)(?:\\s+(.*?))?\\s*(//|$)", str(token).replace("\\\n", ""))
                if match:
                    directive = match.group(1)
                    data = match.group(2)
                    if data: data = ' '.join(re.split('\s-*/\\*.*?\\*/\s-*', data))

                    if conditional and not conditional[-1]:
                        if directive in ("if", "ifdef", "ifndef"):
                            conditional.append(Conditional(False, False))

                        elif directive == "else":
                            if conditional[-1].canTakeBranch(): conditional[-1].takeBranch()

                        elif directive == "elif":
                            if conditional[-1].canTakeBranch():
                                if self.evaluate(token.filename(), token.line, data):
                                    conditional[-1].takeBranch()

                        elif directive == "endif":
                            conditional.pop()

                    elif directive == "ifdef":
                        conditional.append(Conditional(data.strip() in self.__macros))

                    elif directive == "ifndef":
                        conditional.append(Conditional(data.strip() not in self.__macros))

                    elif directive == "if":
                        conditional.append(Conditional(self.evaluate(token.filename(), token.line, data)))

                    elif directive == "elif":
                        if conditional: conditional[-1].endBranch()
                        else: self.error(token.filename(), token.line(), "unexpected #elif directive ignored")

                    elif directive == "else":
                        if conditional: conditional.pop()
                        else: self.error(token.filename(), token.line(), "unexpected #else directive ignored")

                    elif directive == "endif":
                        if conditional: conditional.pop()
                        else: self.error(token.filename(), token.line(), "unexpected #endif directive ignored")

                    elif directive in ("define", "undef", "include"):
                        self.error(token.filename(), token.line(), "#%s not supported by Preprocessor.filterTokens()" % directive)

                    elif directive == "error":
                        self.error(token.filename(), token.line(), "#error: %s" % data)

                    else:
                        self.warning(token.filename(), token.line(), "ignoring unhandled directive #%s" % directive)

            else:
                if not conditional or conditional[-1]: yield token

    def storeTo(self, filename):
        from cPickle import dump
        util.makedirs(os.path.dirname(filename))
        fd, fail = None, True
        try:
            fd = open(filename, "wct")
            dump(sorted(self.__userFiles | self.__systemFiles), fd)
            dump(self.__initialMacros, fd)
            dump(self.__macros, fd)
            fail = False
        finally:
            if fd: fd.close()
            if fail: os.unlink(filename)

    def restoreFrom(self, dbfilename):
        from cPickle import load

        try:
            dbstat = os.stat(dbfilename)
            db = None
            try:
                db = open(dbfilename)
                files = load(db)
                for filename in files:
                    filestat = os.stat(filename)
                    if dbstat[stat.ST_MTIME] < filestat[stat.ST_MTIME]:
                        if self.__verbose:
                            print "not using cached data in '%s' because '%s' has a newer modification time" % (dbfilename, filename)
                        return False

                initialMacros = load(db)

                if initialMacros != self.__initialMacros:
                    if self.__verbose:
                        print "not using cached data in '%s' because externally defined macros differ" % dbfilename
                    return False

                self.__macros = load(db)
            finally:
                if db: db.close()
            for filename in files:
                util.fileTracker.addInput(filename)
            return True
        except:
            if self.__verbose:
                print "not using cached data in '%s' because loading it failed" % dbfilename
            return False

if __name__ == "__main__":
    import sys
    import optparse

    parser = optparse.OptionParser()
    parser.add_option("-D", action="append", dest="defines")
    parser.add_option("-I", action="append", dest="user_include_paths")
    parser.add_option("-q", "--quiet", action="store_true", dest="quiet")
    parser.add_option("-Q", "--ignore-errors", action="store_true", dest="ignore_errors")
    parser.add_option("-P", action="store_true", dest="profile")
    parser.add_option("-v", "--verbose", action="store_true", dest="verbose")
    parser.add_option("--dump", action="store_true", dest="dump")
    parser.add_option("--cache", action="store", dest="dbfilename")
    parser.add_option("-p", "--process", action="append", dest="process")
    parser.add_option("-f", "--filter", action="append", dest="filter")

    options, args = parser.parse_args()

    pp = Preprocessor()

    if options.user_include_paths:
        for path in options.user_include_paths:
            pp.addUserIncludePath(path)

    if options.ignore_errors:
        pp.ignoreErrors()
        pp.setQuiet()
    elif options.quiet:
        pp.setQuiet()
    elif options.verbose:
        pp.setVerbose()

    if options.defines:
        for define in options.defines:
            parts = define.split("=", 1)
            if len(parts) == 1:
                parts.append(None)
            pp[parts[0]] = parts[1]

    if not options.dbfilename or not pp.restoreFrom(options.dbfilename):
        if options.process:
            for filename in options.process:
                try:
                    if options.profile:
                        import profile
                        profile.run("pp.preprocess(filename)")
                    else:
                        pp.preprocess(filename)
                except:
                    import traceback
                    traceback.print_exc()

        if options.dbfilename:
            pp.storeTo(options.dbfilename)

    if options.dump:
        for macro in sorted(pp.macros(), key=Macro.name):
            if macro.value(): print "#define %s %s" % (macro.name(), macro.value())
            else: print "#define %s" % macro.name()

    if options.filter:
        for filename in options.filter:
            try:
                f = None
                try:
                    f = open(filename)
                    for token in sorted(map(str, pp.filterTokens(filename, clexer.tokenize(clexer.split(f.read()))))):
                        if clexer.isidentifier(token):
                            print token
                finally:
                    if f: f.close()
            except:
                import traceback
                traceback.print_exc()
