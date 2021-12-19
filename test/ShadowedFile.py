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
# @file  ShadowedFile utility class.
# 

import os
import io
import tempfile
import random
from cksum import cksumFile, cksumStream

class ShadowedFile(io.RawIOBase):
    '''A file-like object that maintains a shadow copy of file data in a separate temporary file.
       Writing writes to both the primary and shadow files, while reading reads both files and 
       compares the result. The shadow file is automatically deleted when __exit__() is called.
       '''

    def __init__(self, name, shadowDir=None):
        super().__init__()
        self.name = name
        self.primaryFile = open(name, "w+b", buffering=0)
        self.shadowName = None
        self.shadowFile = None
        try:
            self.shadowFile = tempfile.NamedTemporaryFile(dir=shadowDir, buffering=0, delete=False)
            self.shadowName = self.shadowFile.name
        finally:
            if self.shadowFile is None:
                self.primaryFile.close()
                os.remove(name)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        if self.shadowFile is not None:
            f = self.shadowFile
            self.shadowFile = None
            f.close()
        if self.shadowName is not None and os.path.exists(self.shadowName):
            os.remove(self.shadowName)
        self.shadowName = None

    def open(self):
        if self.shadowFile is None:
            raise ValueError('Shadow file no longer exists')
        if self.primaryFile is None:
            self.primaryFile = open(self.name, "r+b", buffering=0)
        self.primaryFile.seek(0)

    def close(self):
        if self.primaryFile is not None:
            f = self.primaryFile
            self.primaryFile = None
            f.close()

    def reopen(self):
        self.close()
        self.open()

    @property
    def closed(self):
        return self.primaryFile is None

    def readable(self):
        return True

    def writable(self):
        return True

    def seekable(self):
        return True

    def readinto(self, b):
        self._checkClosed()
        readLen = len(b)
        primaryBuf = bytearray(readLen)
        shadowBuf = bytearray(readLen)
        readRes = self._readVerifyChunk(primaryBuf, shadowBuf)
        b[0:readRes] = primaryBuf
        return readRes

    def write(self, b):
        self._checkClosed()
        pos = self.primaryFile.tell()
#        print('Write block %d' % (pos / 512))
        self.shadowFile.seek(pos)
        shadowRes = self.shadowFile.write(b)
        primaryRes = self.primaryFile.write(b)
        self.primaryFile.flush()
        assert primaryRes == shadowRes, '%s != %s' % (primaryRes, shadowRes)
        return primaryRes

    def writeRandom(self, len, chunkSize=512):
        while len > 0:
            writeSize = len if len <= chunkSize else chunkSize
            writeRes = self.write(randbytes(writeSize))
            assert writeRes == writeSize, '%s != %s' % (writeRes, writeSize)
            len -= writeSize

    def seek(self, off, whence=0):
        self._checkClosed()
        return self.primaryFile.seek(off, whence)

    def readVerify(self, chunkSize=512):
        self._checkClosed()
        self.primaryFile.seek(0)
        primaryBuf = bytearray(chunkSize)
        shadowBuf = bytearray(chunkSize)
        while self._readVerifyChunk(primaryBuf, shadowBuf) > 0:
            pass

    def truncate(self, size):
        self._checkClosed()
        self.shadowFile.seek(self.primaryFile.tell())
        primaryRes = self.primaryFile.truncate(size)
        shadowRes = self.shadowFile.truncate(size)
        assert primaryRes == shadowRes, '%s != %s' % (primaryRes, shadowRes)
        return primaryRes

    def _readVerifyChunk(self, primaryBuf, shadowBuf):
        pos = self.primaryFile.tell()
        self.shadowFile.seek(pos)
#        print('Read block %d, offset %d' % (pos / 512, pos % 512))
        primaryRes = self.primaryFile.readinto(primaryBuf)
        shadowRes = self.shadowFile.readinto(shadowBuf)
        assert primaryRes == shadowRes, '%s != %s' % (primaryRes, shadowRes)
        if primaryRes > 0:
            diffOffset = next((i for i in range(primaryRes) if primaryBuf[i] != shadowBuf[i]), None)
            if diffOffset != None:
                diffPos = pos + diffOffset
                chunkSize = len(primaryBuf)
                n = diffOffset & ~0xF
                m = n + 32
                if m > primaryRes:
                    m = primaryRes
                primaryData = primaryBuf[n:m].hex()
                shadowData = shadowBuf[n:m].hex()
                diffPtr = ' ' * ((diffOffset & 0xF) * 2) + '^'
                print(f'Mismatched data at file position {diffPos} (0x{diffPos:x})\n'
                     f'  read position = {pos}\n'
                     f'  chunk size = {chunkSize}\n'
                     f'  read result = {primaryRes}\n'
                     f'  diff offset = {diffOffset}\n'
                     f'  expected: {shadowData}\n'
                     f'  actual:   {primaryData}\n'
                     f'            {diffPtr}')
                assert primaryData == shadowData, 'primaryData != shadowData'
        return primaryRes

def randbytes(len):
#    return bytearray(random.getrandbits(8) for _ in range(0, len))
    return bytearray(42 for _ in range(0, len))
