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
# @file  Classes for tracking properties of files created during
#        filesystem tests.
# 

import os
import stat
import itertools
import collections

class FileList(collections.UserList):
    '''A mutable list of FileListEntry objects describing files and
       directories in a filesystem.'''

    def diff(self, other):
        fl1 = sorted(self, key=lambda entry : entry.name)
        fl2 = sorted(other, key=lambda entry : entry.name)
        for (entry1, entry2) in itertools.zip_longest(fl1, fl2):
            if entry1 is None or entry2 is None:
                return (entry1, entry2, None)
            if entry1.name < entry2.name:
                return (entry1, None, None)
            if entry2.name < entry1.name:
                return (None, entry2, None)
            for fieldName in [ 'type', 'mode', 'linkCount', 'uid', 'gid', 'size', 'major', 'minor', 'time', 'cksum' ]:
                field1 = getattr(entry1, fieldName)
                field2 = getattr(entry2, fieldName)
                if field1 is not None and field2 is not None and field1 != field2:
                    return (entry1, entry2, fieldName)
        return None

    def mapNames(self, dirName, newDirName):
        for file in self:
            file.name = os.path.join(newDirName, os.path.relpath(file.name, dirName))

    @classmethod
    def parse(cls, fileListStr, fileListRegex):
        '''Parse a string containing a list of file/directories and their
           metadata using the given regex.'''
        res = cls()
        for m in fileListRegex.finditer(fileListStr):
            f = FileListEntry(**m.groupdict())
            res.append(f)
        return res

class FileListEntry:
    '''Describes a file or directory contained within a filesystem, along
       with details of its metadata.'''

    def __init__(self, name, type, **kwargs):
        self.name = name
        self.type = 'f' if type is '-' else type
        self.mode = kwargs.get('mode', None)
        if self.mode is not None:
            if not isinstance(self.mode, int):
                self.mode = FileListEntry._parseModeStr(self.mode)
            self.mode = stat.S_IMODE(self.mode) # just the permission bits
        self.linkCount = kwargs.get('linkCount', None)
        if self.linkCount is not None and not isinstance(self.linkCount, int):
            self.linkCount = int(self.linkCount)
        self.uid = kwargs.get('uid', None)
        try:
            self.uid = int(self.uid)
        except:
            pass
        self.gid = kwargs.get('gid', None)
        try:
            self.gid = int(self.gid)
        except:
            pass
        self.size = kwargs.get('size', None)
        if self.size is not None and not isinstance(self.size, int):
            self.size = int(self.size)
        self.major = kwargs.get('major', None)
        if self.major is not None and not isinstance(self.major, int):
            self.major = int(self.major)
        self.minor = kwargs.get('minor', None)
        if self.minor is not None and not isinstance(self.minor, int):
            self.minor = int(self.minor)
        self.time = kwargs.get('time', None)
        self.cksum = kwargs.get('cksum', None)
        if self.cksum == '-':
            self.cksum = None
        elif self.cksum is not None and not isinstance(self.cksum, int):
            self.cksum = int(self.cksum)

    def __lt__(self, other):
        return self.name < other.name

    def __le__(self, other):
        return self.name <= other.name

    def __eq__(self, other):
        return self.name == other.name

    def __ne__(self, other):
        return self.name != other.name

    def __gt__(self, other):
        return self.name > other.name

    def __ge__(self, other):
        return self.name >= other.name

    @staticmethod
    def _parseModeStr(modeStr):

        modeStrLen = len(modeStr)
        if modeStrLen < 9 or modeStrLen > 10:
            raise ValueError('Invalid mode: %s' % modeStr)

        mode = 0

        # user
        if modeStr[0] == 'r':
            mode |= stat.S_IRUSR
        elif modeStr[0] != '-':
            raise ValueError('Invalid mode: %s' % modeStr)
        if modeStr[1] == 'w':
            mode |= stat.S_IWUSR
        elif modeStr[1] != '-':
            raise ValueError('Invalid mode: %s' % modeStr)
        if modeStr[2] == 'x':
            mode |= stat.S_IXUSR
        elif modeStr[2] == 's':
            mode |= stat.S_IXUSR | stat.S_ISUID
        elif modeStr[2] == 'S':
            mode |= stat.S_ISUID
        elif modeStr[2] != '-':
            raise ValueError('Invalid mode: %s' % modeStr)

        # group
        if modeStr[3] == 'r':
            mode |= stat.S_IRGRP
        elif modeStr[3] != '-':
            raise ValueError('Invalid mode: %s' % modeStr)
        if modeStr[4] == 'w':
            mode |= stat.S_IWGRP
        elif modeStr[4] != '-':
            raise ValueError('Invalid mode: %s' % modeStr)
        if modeStr[5] == 'x':
            mode |= stat.S_IXGRP
        elif modeStr[5] == 's':
            mode |= stat.S_IXGRP | stat.S_ISGID
        elif modeStr[5] == 'S':
            mode |= stat.S_ISGID
        elif modeStr[5] != '-':
            raise ValueError('Invalid mode: %s' % modeStr)

        # other / sticky
        if modeStr[6] == 'r':
            mode |= stat.S_IROTH
        elif modeStr[6] != '-':
            raise ValueError('Invalid mode: %s' % modeStr)
        if modeStr[7] == 'w':
            mode |= stat.S_IWOTH
        elif modeStr[7] != '-':
            raise ValueError('Invalid mode: %s' % modeStr)
        if modeStr[8] == 'x':
            mode |= stat.S_IXOTH
        elif modeStr[8] == 't':
            mode |= stat.S_ISVTX  | stat.S_IXOTH
        elif modeStr[8] == 'T':
            mode |= stat.S_ISVTX 
        elif modeStr[8] != '-':
            raise ValueError('Invalid mode: %s' % modeStr)

        # optional sticky (v6 version of ls prints this)
        if modeStrLen == 10:
            if modeStr[9] != 't':
                raise ValueError('Invalid mode: %s' % modeStr)
            mode |= stat.S_ISVTX 

        return mode
