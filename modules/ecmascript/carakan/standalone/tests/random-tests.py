from random import Random
from optparse import OptionParser
from subprocess import Popen, PIPE
from os import remove
from traceback import print_exc
from sys import stdout, argv
import os.path
import os

R = Random()

# Make sure we get the same sequence of tests each time.
R.seed(0)

INTEGER_LIKELIHOOD = 0.75
SMALL_INTEGER_LIKELIHOOD = 0.75
COMPARISON_LIKELIHOOD = 0.8

def getTestId():
    s = ""
    for i in range(32):
        s += R.choice("0123456789abcdef")
    return s

BITWISE_OR = 1
BITWISE_XOR = 2
BITWISE_AND = 3
EQUALITY = 4
RELATIONAL = 5
SHIFT = 6
ADDITIVE = 7
MULTIPLICATIVE = 8
UNARY = 9

def generateTest(id, mode):
    class Context:
        def __init__(self, r):
            self.r = r
            self.arguments = []
            self.usedArguments = set()
            self.binaryLikelihood = 0.9
            self.onlyIntegers = r.random() < 0.6

            for n in range(r.randint(1, 8)):
                self.arguments.append("abcdefgh"[n])

    class Argument:
        def generate(self, c, level):
            a = c.r.choice(c.arguments)
            c.usedArguments.add(a)
            return a

    class Literal:
        def generate(self, c, level):
            if mode == "bitwise" or c.r.random() < SMALL_INTEGER_LIKELIHOOD:
                return repr(c.r.randint(-1024, 2048))
            elif c.onlyIntegers or c.r.random() < INTEGER_LIKELIHOOD:
                return repr(c.r.randint(-2147483648, 2147483647))
            else:
                return repr((c.r.random() - 0.5) * 4294967296.0)

    class BinaryExpression:
        def generate(self, c, parentLevel):
            if mode == "numeric" or (mode == "condition" and parentLevel != 0):
                ops = [ ("+", ADDITIVE), ("-", ADDITIVE),
                        ("*", MULTIPLICATIVE), ("/", MULTIPLICATIVE) ]
            elif mode == "condition":
                if c.r.random() < COMPARISON_LIKELIHOOD:
                    ops = [ ("==", EQUALITY), ("!=", EQUALITY), ("===", EQUALITY), ("!==", EQUALITY),
                            ("<", RELATIONAL), ("<=", RELATIONAL), (">", RELATIONAL), (">=", RELATIONAL) ]
                else:
                    ops = [ ("+", ADDITIVE), ("-", ADDITIVE),
                            ("*", MULTIPLICATIVE), ("/", MULTIPLICATIVE) ]
            elif mode == "bitwise":
                ops = [ ("|", BITWISE_OR), ("^", BITWISE_XOR), ("&", BITWISE_AND),
                        ("<<", SHIFT), (">>", SHIFT) ]

            op, level = c.r.choice(ops)
            if parentLevel > level: format = "(%s %s %s)"
            else: format = "%s %s %s"
            return format % (expression(c).generate(c, level), op, expression(c).generate(c, level + 0.5))

    class Negate:
        def generate(self, c, parentLevel):
            e = expression(c).generate(c, UNARY)
            if e[0] == '-': format = "-(%s)"
            else: format = "-%s"
            return format % e

    class Complement:
        def generate(self, c, parentLevel):
            e = expression(c).generate(c, UNARY)
            if e[0] == '~': format = "~(%s)"
            else: format = "~%s"
            return format % e

    def expression(c):        
        if c.r.random() < c.binaryLikelihood:
            c.binaryLikelihood *= 0.9
            return BinaryExpression()
        elif mode == "bitwise":
            return c.r.choice([Argument, Argument, Argument, Argument, Literal, Literal, Complement])()
        else:
            return c.r.choice([Argument, Argument, Argument, Argument, Literal, Literal, Negate])()

    r = Random()
    r.seed(id)
    c = Context(r)

    if mode == "random":
        mode = c.r.choice(["numeric", "condition", "bitwise"])
        lexical_args = c.r.random() < 0.5
    else:
        lexical_args = False

    e = expression(c).generate(c, 0)

    if mode == "condition":
        if lexical_args:
            body = "return (function fn2 () { if (%s) return 'yes'; else return 'no'; })();" % e
        else:
            body = "if (%s) return 'yes'; else return 'no';" % e
        expected = "eval(\"(%s) ? 'yes' : 'no'\")" % e
    else:
        if lexical_args:
            body = "return (function fn2 () { return %s; })();" % e
        else:
            body = "return %s;" % e
        expected = "eval(\"%s\")" % e

    parameters = ", ".join(c.usedArguments)
    arguments = []
    scope = []

    for x in c.usedArguments:
        v = Literal().generate(c, 0)
        arguments.append(v)
        scope.append((x, v))

    variables = "".join(["var %s = %s;\n" % (n, v) for n, v in scope])

    return "function fn(%s)\n{\n  %s\n}\n\n%s\nvar expected = %s;\nvar pass = true;\n\nfor (var i = 0; i < 100; ++i)\n{\n  var result = fn(%s);\n  if (result != expected && String(result) != String(expected))\n  {\n    writeln('fail at iteration ' + i, '  result:   ' + result, '  expected: ' + expected);\n    pass = false;\n    break;\n  }\n}\n\nif (pass)\n  writeln('pass');" % (parameters, body, variables, expected, parameters)

p = OptionParser()
p.add_option("-x", "--executable", dest="executable", help="jsshell executable to test", default="./fast-jsshell")
p.add_option("-i", "--id", dest="id", help="test ID to generate")
p.add_option("-l", "--limit", dest="limit", help="maximum number of failed tests; when reached script exits", type="int", default=-1)
p.add_option("-p", "--print", dest="printTest", help="generate test script to stdout instead of running it (use -i/--id to specify which test!)", action="store_true")
p.add_option("-m", "--mode", dest="mode", help="mode: numeric (default) or condition", type="string", default="random")
p.add_option("-s", "--skip", dest="skip", help="number of test IDs to skip past before starting", type="int", default="0")

(options, arguments) = p.parse_args()

for x in range(options.skip):
    getTestId()

skipList = {}

def readSkipList():
    if os.access(os.path.join(os.path.dirname(__file__), "random-tests-skiplist.txt"), os.R_OK):
        for line in open(os.path.join(os.path.dirname(__file__), "random-tests-skiplist.txt")):
            id, reason = line.strip().split(":", 1)
            skipList[id.strip()] = reason.strip()

readSkipList()

def runTest(executable, id):
    if skipList.has_key(id): return "skipped: " + skipList[id]

    try: test = generateTest(id, options.mode)
    except: return "passed"

    filename = "%s-%s.js" % (options.mode, id)
    file = open(filename, "w")
    print >>file, test

    file.close()

    args = [executable, "-q"] + arguments + [filename]

    try:
        process = Popen(args, stdin=PIPE, stdout=PIPE)
        stdoutdata, stderrdata = process.communicate()

        if process.returncode < 0: return "crashed"
        elif process.returncode > 0: return "failed: " + stdoutdata.strip()
        elif stdoutdata.strip() != "pass": return stdoutdata.strip()
        else:
            remove(filename)
            return "passed"
    except KeyboardInterrupt:
        remove(filename)
        raise

if options.id:
    if options.printTest:
        print generateTest(options.id, options.mode)
    else:
        print options.id + ": " + runTest(options.executable, options.id)
elif options.printTest:
    print generateTest(getTestId(), options.mode)
else:
    failed = 0
    tested = 0

    try:
        print "|",
        stdout.flush()
        while True:
            id = getTestId()
            result = runTest(options.executable, id)
            tested += 1
            if result.startswith("fail") or result == "crashed":
                print "\r%d: %s: %s" % (tested, id, result)
                failed += 1
                if options.limit != -1 and failed == options.limit:
                    break
            elif result != "passed":
                print "\r" + id + ": " + result
                
            if (tested % 100) == 0:
                print "\r%s" % "|/-\\"[tested / 100 % 4],
                stdout.flush()
    except KeyboardInterrupt:
        pass

    print "\ntests run: %d" % tested
