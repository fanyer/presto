# You need jsshell and mjsunit to be able to run this script

import commands
import glob
import sys

def testFunction(jsengine, mjsunit, testscript):
    a = commands.getstatusoutput(jsengine + " -q " +
                                 mjsunit + " " + testscript)
    return a[0] == 0, a[1]

def runTests(jsengine, mjsunit, dir):
    passed = 0
    failed = 0
    passed_tests = []
    failed_tests = []
    failed_tests_msg = []
    files = glob.glob(dir + "*.js")
    files.sort()
    for x in files:
        print "calling " + x
        b = testFunction(jsengine,
                         mjsunit,
                         x)
        if b[0]:
            print "PASSED"
            passed += 1
            passed_tests += [x]
        else:
            print "FAILED"
            failed += 1
            failed_tests += [[x, b[1]]]
    print "------------------------- PASSED TESTS ------------------------"
    for x in passed_tests:
        print x
    print "------------------------- FAILED TESTS ------------------------"
    for x in failed_tests:
        print "---- " +  x[0] + " ----"
        print x[1]
    print "---------------------------------------------------------------"
    print "number of tests passed: ", passed
    print "number of tests failed: ", failed

def main():
    argv = sys.argv
    if len(argv) < 4:
        print("Please provide a path to jsshell, path to mjsunit.js and path to all js tests")
    else:
        runTests(argv[1], argv[2], argv[3])

if __name__ == "__main__":
    main()
