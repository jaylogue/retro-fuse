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
# @file  General filesystem operational tests
# 

import os
import stat
import math
import random
from cksum import cksumFile, cksumStream
from ShadowedFile import ShadowedFile
from FileList import FileList, FileListEntry
from TestUtils import obj

class FileIOTests:
    '''Base class implementing a general set of file I/O tests useful for
       testing ancient filesystems.
       
       This class is intended to be mixed in to a TestCase class for testing
       a specific filesystem type.

       targetDir property specifies root directory in which to perform tests.
       This property must be set by the subclass.

       fileList property accumulates a list of FileListEntry objects describing
       the files created by the test. After the tests are run, these can be used
       to verify the contents of the filesystem as seen from the legacy OS.
       
       testConfig property contains per-test configuration parameters which can be
       overridden by a subclass if necessary.
       '''

    fileList = FileList()

    testConfig = obj({
        'ReadWriteFile' : {
            'FileSizes' : [ 10, 1000, 500000 ],
            'WriteSizes' : [
                [ 1, 3, 10 ],
                [ 13, 256, 1000 ],
                [ 513, 4093, 65536 ],
            ],
            'ReadSizes' : [ 
                [ 1, 3, 10, 512 ],
                [ 1, 3, 10, 512 ],
                [ 11, 1024, 4093, 65536 ],
            ]
        },
        'OverwriteFile' : {
            'FileSizes' : [ 2011, 30303, 166420 ],
            'StripeSizes' : [ 
                [ 10, 5, 2 ],
                [ 512, 200, 64, 11, 8 ],
                [ 32768, 512, 127, 61, 32 ],
            ]
        },
        'TruncateFile' : {
            'FileSizes' : [ 16, 65536, 800*1024 ],
            'Ratios' : [ 2, 10, 0 ]
        },
        'ExtendFile' : {
            'FileSizes' : [ 2000, 3000, 60000, 199999 ]
        },
        'ReadWriteSparseFile' : {
            'FileSizes' : [ 1024, 10000, 500000 ], # TODO: 10000000
            'SkipSizes' : [ 
                [ 32, 100 ],
                [ 512, 127 ],
                [ 32767, 75000 ],
                [ 500000, 1024 * 1024 ]
            ],
            'ReadSizes' : [ 
                [ 1, 3, 10, 512 ],
                [ 1, 3, 10, 512 ],
                [ 11, 1024, 4093, 32768 ],
                [ 65535, 256*1024-1 ]
            ]
        },
        'ChangeFileMode' : {
            'Modes' : [
                stat.S_IRUSR, stat.S_IWUSR, stat.S_IXUSR,
                stat.S_IRGRP, stat.S_IWGRP, stat.S_IXGRP,
                stat.S_IROTH, stat.S_IWOTH, stat.S_IXOTH,
                stat.S_ISUID|stat.S_IXUSR, stat.S_ISGID|stat.S_IXGRP
            ]
        },
        'ChangeDirectoryMode' : {
            'Modes' : [
                stat.S_IRUSR, stat.S_IWUSR, stat.S_IXUSR,
                stat.S_IRGRP, stat.S_IWGRP, stat.S_IXGRP,
                stat.S_IROTH, stat.S_IWOTH, stat.S_IXOTH,
                stat.S_ISVTX|stat.S_IXOTH
            ]
        },
    })

    def setUp(self):
        super().setUp()

        # generate deterministic random data based on the name of the
        # test being invoked.
        random.seed(self.id())


    # ---------- Basic File Tests ----------

    def test_01_CreateFile(self):
        '''Create File'''
        fn = os.path.join(self.targetDir, 'f01')
        with open(fn, "w+") as f:
            pass
        statRes = os.stat(fn)
        self.assertEqual(statRes.st_size, 0)
        self.fileList.append(FileListEntry(fn, 'f', size=0, mode=statRes.st_mode, linkCount=1))

    def test_02_DeleteFile(self):
        '''Delete File'''
        fn = os.path.join(self.targetDir, 'f02')
        with open(fn, "w+") as f:
            pass
        statRes = os.stat(fn)
        self.assertEqual(statRes.st_size, 0)
        os.unlink(fn)
        with self.assertRaises(OSError) as cm:
            os.stat(fn)

    def test_03_RenameFile(self):
        '''Rename File'''
        fn1 = os.path.join(self.targetDir, 'f03-1')
        fn2 = os.path.join(self.targetDir, 'f03-2')
        with open(fn1, "w+") as f1:
            pass
        stat1 = os.stat(fn1)
        self.assertEqual(stat1.st_size, 0)
        os.rename(fn1, fn2)
        with self.assertRaises(OSError) as cm:
            os.stat(fn1)
        stat2 = os.stat(fn2)
        self.assertEqual(stat2.st_size, 0)
        self.assertEqual(stat2.st_mode, stat1.st_mode)
        # TODO: Test preservation mod time?
        self.fileList.append(FileListEntry(fn2, 'f', size=0, mode=stat2.st_mode, linkCount=1))

    def test_04_CreateFileLink(self):
        '''Create File Link'''
        fn1 = os.path.join(self.targetDir, 'f04-1')
        fn2 = os.path.join(self.targetDir, 'f04-2')
        fn3 = os.path.join(self.targetDir, 'f04-3')
        with open(fn1, "w+") as f1:
            pass
        stat1 = os.stat(fn1)
        self.assertEqual(stat1.st_size, 0)
        os.link(fn1, fn2)
        stat2 = os.stat(fn2)
        self.assertEqual(stat2.st_size, 0)
        stat1 = os.stat(fn1)
        self.assertEqual(stat1.st_size, 0)
        os.link(fn2, fn3)
        stat3 = os.stat(fn3)
        self.assertEqual(stat3.st_size, 0)
        stat2 = os.stat(fn2)
        self.assertEqual(stat2.st_size, 0)
        self.fileList.append(FileListEntry(fn1, 'f', size=0, mode=stat1.st_mode, linkCount=3))
        self.fileList.append(FileListEntry(fn2, 'f', size=0, mode=stat2.st_mode, linkCount=3))
        self.fileList.append(FileListEntry(fn3, 'f', size=0, mode=stat3.st_mode, linkCount=3))

    def test_05_DeleteFileLink(self):
        '''Delete File Link'''
        fn1 = os.path.join(self.targetDir, 'f05-1')
        fn2 = os.path.join(self.targetDir, 'f05-2')
        with open(fn1, "w+") as f1:
            pass
        stat1 = os.stat(fn1)
        self.assertEqual(stat1.st_size, 0)
        os.link(fn1, fn2)
        os.unlink(fn1)
        with self.assertRaises(OSError) as cm:
            os.stat(fn1)
        stat2 = os.stat(fn2)
        self.assertEqual(stat2.st_size, 0)
        self.fileList.append(FileListEntry(fn2, 'f', size=0, mode=stat2.st_mode, linkCount=1))

    def test_06_ChangeFileMode(self):
        '''Change File Mode'''
        config = self.testConfig.ChangeFileMode
        for i in range(0, len(config.Modes)):
            mode = config.Modes[i]
            fn = os.path.join(self.targetDir, 'f06-%d' % (i+1))
            with open(fn, "w+") as f:
                pass
            os.chmod(fn, mode)
            statRes = os.stat(fn)
            self.assertEqual(stat.S_IMODE(statRes.st_mode), mode)
            self.fileList.append(FileListEntry(fn, 'f', size=0, mode=statRes.st_mode, linkCount=1))

    # ---------- File Read/Write Tests ----------

    def test_10_ReadWriteFile(self):
        '''Read/Write File'''
        config = self.testConfig.ReadWriteFile
        # for each of various file sizes...
        for i in range(0, len(config.FileSizes)):
            fileName = os.path.join(self.targetDir, 'f10-%d' % (i+1))
            fileSize = config.FileSizes[i]
            readSizes = config.ReadSizes[i]
            writeSizes = config.WriteSizes[i]
            for writeSize in writeSizes:
                if os.path.exists(fileName):
                    os.remove(fileName)
                with ShadowedFile(fileName) as f:
                    # write the file
                    f.writeRandom(fileSize, chunkSize=writeSize)
                    # verify the reported size of the file on disk
                    stat1 = os.stat(fileName)
                    self.assertEqual(stat1.st_size, fileSize)
                    # read and verify the contents of the file, in various size chunks
                    for readSize in readSizes:
                        f.reopen()
                        f.readVerify(chunkSize=readSize)
            # compute the file's checksum
            expectedCksum = cksumFile(fileName)[0]
            # add the file to the list of generated files.
            self.fileList.append(FileListEntry(fileName, 'f', size=fileSize, linkCount=1, cksum=expectedCksum))

    def test_11_OverwriteFile(self):
        '''Overwrite File'''
        config = self.testConfig.OverwriteFile
        # for each of various file sizes...
        for i in range(0, len(config.FileSizes)):
            fileName = os.path.join(self.targetDir, 'f11-%d' % (i+1))
            fileSize = config.FileSizes[i]
            stripeSizes = config.StripeSizes[i]
            with ShadowedFile(fileName) as f:
                # write the file
                f.writeRandom(fileSize)
                # for each stripe size, overwrite the file in stripes spaced
                # equidistant from each other.  verify the contents of the
                # file at each pass.
                for stripeSize in stripeSizes:
                    f.reopen()
                    writePos = 0
                    while writePos < fileSize:
                        remainingSize = fileSize - writePos
                        writeSize = remainingSize if remainingSize < stripeSize else stripeSize
                        f.writeRandom(writeSize, chunkSize=writeSize)
                        writePos = f.seek(stripeSize, os.SEEK_CUR) # seek to next stripe
                    f.readVerify()
                # compute the file's final checksum
                f.seek(0)
                expectedCksum = cksumStream(f)[0]
                # add the file to the list of generated files.
                self.fileList.append(FileListEntry(fileName, 'f', size=fileSize, linkCount=1, cksum=expectedCksum))

    def test_12_ExtendFile(self):
        '''Extend File'''
        config = self.testConfig.ExtendFile
        # for each of various file sizes...
        for i in range(0, len(config.FileSizes)):
            fileSize = config.FileSizes[i]
            # create a file of the target size and then extend it to half again
            # its initial size.
            fileName = os.path.join(self.targetDir, 'f12-%d' % (i+1))
            with ShadowedFile(fileName) as f:
                extendSize = int(math.floor(fileSize / 2))
                newFileSize = fileSize + extendSize
                f.writeRandom(fileSize)
                f.reopen()
                f.seek(0, whence=os.SEEK_END)
                f.writeRandom(extendSize)
                f.close()
                # verify file size on disk
                s = os.stat(fileName)
                self.assertEqual(s.st_size, newFileSize)
                # verify the file's contents
                f.open()
                f.readVerify()
                # compute the file's final checksum
                f.seek(0)
                expectedCksum = cksumStream(f)[0]
                # add the file to the list of generated files.
                self.fileList.append(FileListEntry(fileName, 'f', size=newFileSize, linkCount=1, cksum=expectedCksum))

    def test_13_TruncateFile(self):
        '''Truncate File'''
        config = self.testConfig.TruncateFile
        # for each of various file sizes...
        for i in range(0, len(config.FileSizes)):
            fileSize = config.FileSizes[i]
            # for each of various truncation ratios...
            for ratio in config.Ratios:
                # create a file of the target size and then truncate it based
                # on the current size ratio
                fileName = os.path.join(self.targetDir, 'f13-%d-%d' % (i+1, ratio))
                with ShadowedFile(fileName) as f:
                    f.writeRandom(fileSize)
                    f.reopen()
                    newFileSize = int(math.floor(fileSize / ratio)) if ratio > 0 else 0
                    f.truncate(newFileSize)
                    f.close()
                    # verify file size on disk
                    s = os.stat(fileName)
                    self.assertEqual(s.st_size, newFileSize)
                    # verify the file's contents
                    f.open()
                    f.readVerify()
                    # compute the file's final checksum
                    f.seek(0)
                    expectedCksum = cksumStream(f)[0]
                    # add the file to the list of generated files.
                    self.fileList.append(FileListEntry(fileName, 'f', size=newFileSize, linkCount=1, cksum=expectedCksum))

    def test_14_ReadWriteSparseFile(self):
        '''Read/Write Sparse File'''
        config = self.testConfig.ReadWriteSparseFile
        chunkSize = 512
        # for each of various file sizes...
        for i in range(0, len(config.FileSizes)):
            fileSize = config.FileSizes[i]
            readSizes = config.ReadSizes[i]
            skipSizes = config.SkipSizes[i]
            fileName = os.path.join(self.targetDir, 'f14-%d' % (i+1))
            # for each skip size...
            for skipSize in skipSizes:
                if os.path.exists(fileName):
                    os.remove(fileName)
                with ShadowedFile(fileName) as f:
                    # write the file in 512 byte chunks, skipping skipSize spans
                    # between chunks, up to the target file size, but always
                    # writting the last bytes in the file to ensure the proper length.
                    remainingSize = fileSize
                    while remainingSize > 0:
                        if remainingSize > skipSize:
                            f.seek(skipSize, os.SEEK_CUR)
                            remainingSize -= skipSize
                        else:
                            f.seek(fileSize-1, os.SEEK_SET)
                            remainingSize = 1
                        writeSize = remainingSize if remainingSize < chunkSize else chunkSize
                        f.writeRandom(writeSize)
                        remainingSize -= writeSize
                    # verify the reported size of the file on disk
                    stat1 = os.stat(fileName)
                    self.assertEqual(stat1.st_size, fileSize)
                    # read and verify the contents of the file, in various size chunks
                    for readSize in readSizes:
                        f.reopen()
                        f.readVerify(chunkSize=readSize)
            # compute the file's final checksum
            expectedCksum = cksumFile(fileName)[0]
            # add the file to the list of generated files.
            self.fileList.append(FileListEntry(fileName, 'f', size=fileSize, linkCount=1, cksum=expectedCksum))

    # ---------- Directory Tests ----------

    def test_20_CreateDirectory(self):
        '''Create Directory'''
        dn = os.path.join(self.targetDir, 'd20')
        os.mkdir(dn)
        statRes = os.stat(dn)
        self.assertTrue(stat.S_ISDIR(statRes.st_mode))
        self.fileList.append(FileListEntry(dn, 'd', mode=statRes.st_mode))
        for i in range(0, 20):
            fn = os.path.join(dn, "f20-%d" % (i+1))
            with open(fn, "w+") as f:
                pass
            statRes = os.stat(fn)
            self.assertEqual(statRes.st_size, 0)
            self.fileList.append(FileListEntry(fn, 'f', size=0, mode=statRes.st_mode, linkCount=1))

    def test_21_RemoveDirectory(self):
        '''Remove Directory'''
        dn = os.path.join(self.targetDir, 'd21-1')
        os.mkdir(dn)
        statRes = os.stat(dn)
        self.assertTrue(stat.S_ISDIR(statRes.st_mode))
        os.rmdir(dn)
        with self.assertRaises(OSError) as cm:
            os.stat(dn)
        dn = os.path.join(self.targetDir, 'd21-2')
        os.mkdir(dn)
        statRes = os.stat(dn)
        self.assertTrue(stat.S_ISDIR(statRes.st_mode))
        self.fileList.append(FileListEntry(dn, 'd', mode=statRes.st_mode))
        fn = os.path.join(dn, "f21")
        with open(fn, "w+") as f:
            pass
        statRes = os.stat(fn)
        self.assertEqual(statRes.st_size, 0)
        self.fileList.append(FileListEntry(fn, 'f', size=0, mode=statRes.st_mode, linkCount=1))
        with self.assertRaises(OSError) as cm:
            os.rmdir(dn)

    def test_22_RenameDirectory(self):
        '''Rename Directory'''
        dn = os.path.join(self.targetDir, 'd22-1')
        os.mkdir(dn)
        statRes = os.stat(dn)
        self.assertTrue(stat.S_ISDIR(statRes.st_mode))
        fn = os.path.join(dn, "f22")
        with open(fn, "w+") as f:
            pass
        statRes = os.stat(fn)
        self.assertEqual(statRes.st_size, 0)
        dn2 = os.path.join(self.targetDir, 'd22-2')
        os.rename(dn, dn2)
        statRes = os.stat(dn2)
        self.assertTrue(stat.S_ISDIR(statRes.st_mode))
        self.fileList.append(FileListEntry(dn2, 'd', mode=statRes.st_mode))
        fn2 = os.path.join(dn2, "f22")
        statRes = os.stat(fn2)
        self.assertEqual(statRes.st_size, 0)
        self.fileList.append(FileListEntry(fn2, 'f', size=0, mode=statRes.st_mode, linkCount=1))

    def test_23_CreateNestedDirectories(self):
        '''Create Nested Directories'''
        dn = self.targetDir
        for i in range(0, 10):
            dn = os.path.join(dn, 'd23-%d' % (i+1))
            os.mkdir(dn)
            statRes = os.stat(dn)
            self.assertTrue(stat.S_ISDIR(statRes.st_mode))
            self.fileList.append(FileListEntry(dn, 'd', mode=statRes.st_mode))

    def test_24_ChangeDirectoryMode(self):
        '''Change Directory Mode'''
        config = self.testConfig.ChangeDirectoryMode
        for i in range(0, len(config.Modes)):
            mode = config.Modes[i]
            dn = os.path.join(self.targetDir, 'd24-%d' % (i+1))
            os.mkdir(dn)
            os.chmod(dn, mode)
            statRes = os.stat(dn)
            self.assertEqual(stat.S_IMODE(statRes.st_mode), mode)
            self.fileList.append(FileListEntry(dn, 'd', mode=statRes.st_mode))

