from optparse import OptionParser
from os import access, X_OK, F_OK, R_OK
import os.path
from sys import exit, stderr
from subprocess import Popen, PIPE, STDOUT
from re import search, MULTILINE

parser = OptionParser()

parser.add_option("-x", "--executable", dest="executable", default="./fast-jsshell", help="jsshell executable to use")
parser.add_option("-f", "--file", dest="file", default=None, help="file containing list of benchmarks to run")
parser.add_option("-i", "--iterations", type="int", dest="iterations", default=20, help="number of times each test is run")
parser.add_option("-s", "--sunspider", action="store_true", dest="sunspider", default=False, help="run sunspider tests and output result as sunspider results URL")
parser.add_option("-S", "--sunspider-hq", action="store_true", dest="sunspider_hq", default=False, help="run sunspider tests and output result as sunspider results URL")
parser.add_option("-8", "--v8", action="store_true", dest="v8", default=False, help="run v8 tests and output result as sunspider results URL")
parser.add_option(      "--v8-hq", action="store_true", dest="v8_hq", default=False, help="run v8 tests and output result as sunspider results URL")
parser.add_option("-k", "--kraken", action="store_true", dest="kraken", default=False, help="run kraken tests and output result as sunspider results URL")
parser.add_option("-K", "--kraken-hq", action="store_true", dest="kraken_hq", default=False, help="run kraken tests and output result as sunspider results URL")
parser.add_option("-U", "--URL-format", action="store_true", dest="url_format", default=False, help="run tests from test list and output result as sunspider results URL")
parser.add_option("-p", "--path", type="string", dest="path", default="sunspider", help="path to directory containing sunspider tests")
parser.add_option("-v", "--verbose", action="store_true", dest="verbose", help="be more verbose")
parser.add_option("-l", "--local", action="store_true", dest="local", help="emit local sunspider URL")
parser.add_option("-n", "--native", action="store_true", dest="native", help="use -native-dispatcher to jsshell")
parser.add_option("-c", "--custom", dest="custom", help="custom output item to measure")
parser.add_option("-C", "--custom-div", dest="customdiv", default="1024")
parser.add_option("-T", "--total", action="store_true", dest="total", default=False, help="compute total execution time")
parser.add_option("-a", "--arguments", type="string", dest="arguments", default="", help="arguments to the standalone")

(options, arguments) = parser.parse_args()

if not access(options.executable, F_OK):
    print >>stderr, "%s: no such file" % options.executable
    exit(1)
elif not access(options.executable, X_OK):
    print >>stderr, "%s: not an executable" % options.executable
    exit(1)

divisor = options.customdiv
if not divisor:
    divisor = 1
divisor = int(divisor)

pattern = options.custom
if not pattern:
    pattern = "Execution time"
    divisor = 1

if not options.sunspider and not options.sunspider_hq and not options.v8 and not options.v8_hq and not options.kraken and not options.kraken_hq:
    if options.file is None:
        print >>stderr, "Use -f FILE or --file=FILE to specify a list of paths to testcases to run, one per line."
        exit(1)
    elif not access(options.file, F_OK):
        print >>stderr, "%s: no such file" % options.file
        exit(1)
    elif not access(options.file, R_OK):
        print >>stderr, "%s: not readable" % options.file
        exit(1)

    if options.file.endswith(".js") or options.file.endswith(".es"):
        files = [options.file]
    else:
        files = map(str.strip, open(options.file))
        toremove = []
        for file in files:
            if file.startswith("#"):
                toremove.append(file)
                continue
            file = file.partition(" ")[0]
            if not (file.endswith(".js") or file.endswith(".es")):
                file = os.path.join(options.path, file + ".js")
            if not access(file, F_OK):
                print >>stderr, "%s: no such file" % file
                exit(1)
            elif not access(options.file, R_OK):
                print >>stderr, "%s: not readable" % file
                exit(1)
        for file in toremove:
            files.remove(file)

def run(file, extra_arguments, input_pipe):
    child = Popen([options.executable] + arguments + extra_arguments + [file], stdout=PIPE, stderr=PIPE, stdin=PIPE)
    if input_pipe != None:
        argfile = open(input_pipe);
        for line in argfile:
            child.stdin.write(line)
    (stdout, stderr) = child.communicate()
    if child.returncode == 0:
        m = search("^" + pattern + "\\s*:\\s+([0-9]+(?:\\.[0-9]+)?).*$", stdout.strip().replace("\r\n", "\n"), MULTILINE)
        if m: return float(m.group(1)) / divisor
        else: return "failed"
    else: return "crashed"

iterations = options.iterations

def format(number):
    if number > 1000:
        return "%#.1f" % number
    else:
        return "%.3g" % number

if options.sunspider or options.sunspider_hq or options.v8 or options.v8_hq or options.url_format or options.kraken or options.kraken_hq:
    if (options.sunspider or options.v8 or options.kraken) and iterations != 5:
        print "Note: using --iterations=5 because of --sunspider/--v8/--kraken."
        iterations = 5

    sunspider_files = ["3d-cube", "3d-morph", "3d-raytrace",
                       "access-binary-trees", "access-fannkuch", "access-nbody", "access-nsieve",
                       "bitops-3bit-bits-in-byte", "bitops-bits-in-byte", "bitops-bitwise-and", "bitops-nsieve-bits",
                       "controlflow-recursive",
                       "crypto-aes", "crypto-md5", "crypto-sha1",
                       "date-format-tofte", "date-format-xparb",
                       "math-cordic", "math-partial-sums", "math-spectral-norm",
                       "regexp-dna",
                       "string-base64", "string-fasta", "string-tagcloud", "string-unpack-code", "string-validate-input"]

    v8_files = ["v8-crypto", "v8-deltablue", "v8-earley-boyer", "v8-raytrace", "v8-regexp", "v8-richards", "v8-splay"]

    kraken_files = [ "kraken-ai-astar", "kraken-audio-beat-detection", "kraken-audio-dft", "kraken-audio-fft",
                     "kraken-audio-oscillator", "kraken-imaging-darkroom", "kraken-imaging-desaturate",
                     "kraken-imaging-gaussian-blur", "kraken-json-parse-financial", "kraken-json-stringify-tinderbox",
                     "kraken-stanford-crypto-aes", "kraken-stanford-crypto-ccm", "kraken-stanford-crypto-pbkdf2",
                     "kraken-stanford-crypto-sha256-iterative" ]

    if options.sunspider or options.sunspider_hq:
        files = sunspider_files
    elif options.v8 or options.v8_hq:
        files = v8_files
    elif options.kraken or options.kraken_hq:
        files = kraken_files

    results = []
    
    for file in files:
        times = []
        file, sep, input = file.partition(" ")
        if input != "":
            input = os.path.join(options.path, input)
            if not access(input, F_OK) or not access(input, R_OK):
                print "%s: file doesn't exist or isn't readable" % input
        else:
            input = None
        native = False
        if options.verbose: print file
        if options.native:      
            for n in range(iterations):
                time = run(os.path.join(options.path, file + ".js"), ["-native-dispatcher"],input)
                
                if time == "failed":
                    print "%s native failed" % file
                    break
                elif time == "crashed":
                    print "%s native crashed" % file
                    break
                else:
                    times.append(time)
            else:
                times = sorted(times)[:5]
                results.append("%%22%s%%22:%%5B%s%%5D" % (file, ",".join([format(t) for t in times])))
                continue

        times = []
        
        for n in range(iterations):
            time = run(os.path.join(options.path, file + ".js"), [], input)
            if time == "failed":
                print "%s failed" % file
                results.append("%%22%s%%22:%%5B100,100,100,100,100%%5D" % file)
                break
            elif time == "crashed":
                print "%s crashed" % file
                results.append("%%22%s%%22:%%5B100,100,100,100,100%%5D" % file)
                break
            else:
                times.append(time)
        else:
            times = sorted(times)[:5]
            results.append("%%22%s%%22:%%5B%s%%5D" % (file, ",".join([format(t) for t in times])))
    if options.v8 or options.v8_hq:
        print "http://keno.linkoping.osa/~danielsp/sunspider-v8/sunspider-results.html?%7B" + ",".join(results) + "%7D"
    else:
        print "http://keno.linkoping.osa/~danielsp/sunspider/sunspider-results.html?%7B" + ",".join(results) + "%7D"

else:
    if options.native: extra_arguments = ["-native-dispatcher"]
    else: extra_arguments = []

    extra_arguments.extend(options.arguments.split())

    total = 0

    for file in files:
        if options.verbose: print file
        file, sep, input = file.partition(" ")
        if input != "":
            if not access(input, F_OK) or not access(input, R_OK):
                print "%s: file doesn't exist or isn't readable" % input
        else:
            arg = None
        results = []
        for n in range(options.iterations):
            result = run(file, extra_arguments, arg)
            if result == "failed":
                print "%s:%s failed" % (file, " " * (60 - len(file)))
                break
            elif result == "crashed":
                print "%s:%s crashed" % (file, " " * (60 - len(file)))
                break
            else:
                results.append(result)
        else:
            if options.iterations >= 3:
                lowest = min(results)
                highest = max(results)
                results.remove(lowest)
                results.remove(highest)

            if options.total:
                total = total + sum(results)
            else:
                average = sum(results) / len(results)
                diffdown = (average - min(results)) / average
                diffup = (max(results) - average) / average
                print "%s:%s %.2f ms (+/- %.2f %%)" % (file, " " * (60 - len(file)), average, 100 * max(diffdown, diffup))
    else:
        if options.total:
            print total
