#!/usr/bin/env python3

#
# Copyright 2021 Jay Logue
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 

# 
# @file  A test driver for testing retro-fuse filesystem handlers.
# 

import os
import sys
import unittest
import argparse

scriptName = os.path.basename(__file__)
scriptDirName = os.path.dirname(os.path.abspath(os.path.realpath(__file__)))

class TestResult(unittest.TestResult):

    def __init__(self, stream, descriptions, verbosity):
        super(TestResult, self).__init__(stream, descriptions, verbosity)
        self.stream = stream

    def getDescription(self, test):
        return test.shortDescription()

    def startTest(self, test):
        super(TestResult, self).startTest(test)
        self.stream.write(self.getDescription(test))
        self.stream.write(" ... ")
        self.stream.flush()

    def addSuccess(self, test):
        super(TestResult, self).addSuccess(test)
        self.stream.writeln("PASS")

    def addError(self, test, err):
        super(TestResult, self).addError(test, err)
        self.stream.writeln("ERROR")

    def addFailure(self, test, err):
        super(TestResult, self).addFailure(test, err)
        self.stream.writeln("FAIL")

    def addSkip(self, test, reason):
        super(TestResult, self).addSkip(test, reason)
        self.stream.writeln("skipped {0!r}".format(reason))

    def addExpectedFailure(self, test, err):
        super(TestResult, self).addExpectedFailure(test, err)
        self.stream.writeln("expected failure")

    def addUnexpectedSuccess(self, test):
        super(TestResult, self).addUnexpectedSuccess(test)
        self.stream.writeln("unexpected success")

    def printErrors(self):
        self.stream.writeln()
        self.printErrorList('ERROR', self.errors)
        self.printErrorList('FAIL', self.failures)

    def printErrorList(self, flavour, errors):
        for test, err in errors:
            self.stream.writeln("%s: %s" % (flavour, self.getDescription(test)))
            self.stream.writeln("%s" % err)

# Parse command line arguments
argParser = argparse.ArgumentParser()
argParser.add_argument('-s', '--simh', dest='simhCmd', default='pdp11',
                        help='Path to pdp11 simh executable')
argParser.add_argument('-v', '--verbose', dest='verbosity', action='store_const', const=2, default=1,
                        help='Verbose output')
argParser.add_argument('-q', '--quiet', dest='verbosity', action='store_const', const=0,
                        help='Quiet output')
argParser.add_argument('-f', '--failfast', dest='failfast', action='store_true', default=False,
                        help='Stop on first test failure')
argParser.add_argument('-k', '--keep', dest='keepFS', action='store_true', default=False,
                        help='Retain the test filesystem on exit')
argParser.add_argument('-i', '--fs-image', dest='fsImage',
                        help='Use specified file/device as backing store for test filesystem (implies -k)')
argParser.add_argument('fsHandler', help='Filesystem handler executable to be tested')
testOpts = argParser.parse_args()
if testOpts.fsImage is not None:
    testOpts.keepFS = True

# Verify access to filesystem handler executable
if not os.access(testOpts.fsHandler, os.F_OK):
    print(f'{scriptName}: File not found: {testOpts.fsHandler}', file=sys.stderr)
    sys.exit(1)
if not os.access(testOpts.fsHandler, os.X_OK):
    print(f'{scriptName}: Unable to execute filesystem handler: {testOpts.fsHandler}', file=sys.stderr)
    sys.exit(1)

# Load the appropriate test cases
fsHandlerBaseName = os.path.basename(testOpts.fsHandler)
if fsHandlerBaseName == 'bsd29fs':
    import BSD29Tests
    testSuite = unittest.TestLoader().loadTestsFromModule(BSD29Tests)
elif fsHandlerBaseName == 'v7fs':
    import V7Tests
    testSuite = unittest.TestLoader().loadTestsFromModule(V7Tests)
elif fsHandlerBaseName == 'v6fs':
    import V6Tests
    testSuite = unittest.TestLoader().loadTestsFromModule(V6Tests)
else:
    print(f'{scriptName}: Unknown filesystem handler: {testOpts.fsHandler}', file=sys.stderr)
    print('Expected a file named v6fs, v7fs or bsd29fs', file=sys.stderr)
    sys.exit(1)

# Run the tests
if testOpts.verbosity > 0:
    resultStream = sys.stderr
else:
    resultStream = open(os.devnull, 'a')
testRunner = unittest.TextTestRunner(stream=resultStream, resultclass=TestResult, verbosity=testOpts.verbosity, failfast=testOpts.failfast)
result = testRunner.run(testSuite)
sys.exit(0 if result.wasSuccessful() else 1)
