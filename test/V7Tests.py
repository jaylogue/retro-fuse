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
# @file  Test cases for testing the retro-fuse v7 Unix filesystem handler.
# 

import sys
import unittest
from fractions import Fraction
from SimhDrivers import V7SimhDriver
from FileList import FileListEntry
from FileIOTests import FileIOTests
from RetroFuseTestBase import RetroFuseTestBase

from __main__ import testOpts

class V7FileIOTests(RetroFuseTestBase, FileIOTests, unittest.TestCase):
    '''File I/O tests for retro-fuse v7 Unix filesystem handler.
       Note that the majority of tests are implemented in the FileIOTests base class.
    '''

    fsInitOpts = [ '-oinitfs' ]
    fsMountOpts = [ '-ofssize=153406,fsoffset=18392,use_ino' ]

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        super().initFS()

    def setUp(self):
        super().setUp()
        self.targetDir = type(self).mountDir

        # v7 filesystem only supports truncation to 0
        self.testConfig.TruncateFile.Ratios = [ 0 ]

    def test_99_FilesystemCheck(self):
        '''Filesystem Check
           Mounts the filesystem in a simulated v7 Unix system and verifies
           the integrity and contents of the filesystem against the expected
           list of files/directories left by the file I/O tests.
           '''

        # Unmount the test filesystem from the host system
        type(self).unmountFS()

        # Adjust the list of files generated by the file ops tests such that they
        # match what is expected to be seen when the filesystem is mounted on a
        # v7 system.
        self.fileList.mapNames(self.mountDir, '/mnt')
        self.fileList.insert(0, FileListEntry('/mnt', type='d'))

        # Launch a simulated v7 system with the test filesystem image attached to
        # /dev/rp1-p6 and perform the following actions...
        debugStream = sys.stderr if testOpts.verbosity >= 2 else None
        with V7SimhDriver(simhCmd=testOpts.simhCmd,
                          cwd=self.tempDir,
                          testDiskImage=self.fsImage, 
                          debugStream=debugStream) as v7:

            # Invoke the v7 icheck program on the test filesystem image and fail if 
            # any errors are reported.
            (fileCount, dirCount, blockDevCount, charDevCount, usedBlockCount, freeBlockCount, icheckErrs) = v7.icheckFS('/dev/rp1-p6')
            self.assertIsNone(icheckErrs)

            # Instruct the simulated system to mount the test filesystem
            v7.sendShellCommands('/etc/mount /dev/rp1-p6 /mnt')

            # Enumerate the files and directories in the test filesystem, as seen by
            # the simulated system, along with their metadata and a checksum of their
            # contents.
            v7FileList = v7.enumFiles('/mnt')

        # Compare the files observed in the simulated system to the list of files generated
        # by the file ops tests.  Fail if there are any differences.
        self.assertFilesystemContents(expectedFileList=self.fileList,
                                      observedFileList=v7FileList)

        # Compute the expected number of files/directories in use and fail if this does
        # not match the numbers returned by icheck.
        expectedFileCount = sum(Fraction(1, entry.linkCount) for entry in self.fileList if entry.type == 'f')
        expectedFileCount += 1 # +1 for hidden bad block file (inode 1)
        self.assertEqual(fileCount, expectedFileCount)
        expectedDirCount = sum(1 for entry in self.fileList if entry.type == 'd')
        self.assertEqual(dirCount, expectedDirCount)
        expectedBlockDevCount = sum(1 for entry in self.fileList if entry.type == 'b' or entry.type == 'c')
        self.assertEqual(blockDevCount, expectedBlockDevCount)
        expectedCharDevCount = sum(1 for entry in self.fileList if entry.type == 'b' or entry.type == 'c')
        self.assertEqual(charDevCount, expectedCharDevCount)
