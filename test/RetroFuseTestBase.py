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
# @file  RetroFuseTestBase class
# 

import os
import sys
import subprocess
import tempfile

from __main__ import testOpts

class RetroFuseTestBase:
    '''Common base class for testing retro-fuse filesystem handlers'''

    tempDir = None
    mountDir = None
    fsImage = None
    fsMountOpts = [ ]
    fsInitOpts = [ ]
    fsMounted = False
    keepTmpDir = False

    @classmethod
    def mountFS(cls, fsHandlerOpts=None):
        if cls.fsMounted:
            raise Exception('Filesystem already mounted')
        fsHandler = os.path.abspath(testOpts.fsHandler)
        if fsHandlerOpts is None:
            fsHandlerOpts = cls.fsMountOpts
        args = [ fsHandler ] + fsHandlerOpts + [ cls.fsImage, cls.mountDir ]
        if testOpts.verbosity >= 2:
            print('Mounting filesystem: %s' % ' '.join(args), file=sys.stderr)
        res = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if res.returncode != 0:
            msg = f'Filesystem handler failed with exitcode {res.returncode}\n----------\n{res.stdout}\n----------\n'
            raise Exception(msg)
        cls.fsMounted = True

    @classmethod
    def initFS(cls):
        cls.mountFS(fsHandlerOpts=cls.fsInitOpts)

    @classmethod
    def unmountFS(cls):
        if cls.fsMounted:
            args = [ '/bin/fusermount', '-u', cls.mountDir ]
            if testOpts.verbosity >= 2:
                print('Unmounting filesystem: %s' % ' '.join(args), file=sys.stderr)
            res = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if res.returncode != 0:
                msg = f'fusermount -u failed with exitcode {res.returncode}\n----------\n{res.stdout}\n----------\n'
                raise Exception(msg)
            cls.fsMounted = False

    @classmethod
    def setUpClass(cls) -> None:
        # Create a temporary directory to hold the test files/directory
        cls.tempDir = tempfile.mkdtemp()
        # Create a mount point in the temp directory for the filesystem
        cls.mountDir = os.path.join(cls.tempDir, 'mnt')
        os.mkdir(cls.mountDir, 0o777)
        # If a specific filesystem image has been set, use that. Otherwise, 
        # arrange to store the test filesystem image in a file called
        # fs.dsk in the temporary directory.
        if testOpts.fsImage is not None:
            cls.fsImage = os.path.abspath(testOpts.fsImage)
        else:
            cls.fsImage = os.path.join(cls.tempDir, 'fs.dsk')
            cls.keepTmpDir = testOpts.keepFS
        cls.fsMounted = False

    @classmethod
    def tearDownClass(cls):
        cls.unmountFS()
        if cls.mountDir is not None and os.path.exists(cls.mountDir):
            os.rmdir(cls.mountDir)
        if cls.fsImage is not None:
            if testOpts.keepFS:
                print('Retaining test filesystem in %s' % cls.fsImage, file=sys.stderr)
            elif os.path.exists(cls.fsImage):
                os.remove(cls.fsImage)
        if cls.tempDir is not None and not cls.keepTmpDir and os.path.exists(cls.tempDir):
            os.rmdir(cls.tempDir)

    def assertFilesystemContents(self, expectedFileList, observedFileList):
        diff = expectedFileList.diff(observedFileList)
        if diff is not None:
            (entry1, entry2, fieldName) = diff
            if entry1 is None:
                self.fail("Unexpected file found in filesystem: %s" % entry2.name)
            if entry2 is None:
                self.fail("Expected file not found in filesystem: %s" % entry1.name)
            val1 = getattr(entry1, fieldName)
            val2 = getattr(entry2, fieldName)
            if fieldName == 'cksum':
                fieldName = 'content checksum'
            if fieldName == 'mode':
                val1 = '0%o' % val1
                val2 = '0%o' % val2
            self.fail("File %s in filesystem does not match expected value: %s (expected = %s, actual = %s)" % 
                      (fieldName, entry1.name, val1, val2))
