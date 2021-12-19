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
# @file  Utility classes running and interacting with the simh simulator.
# 

import os
import re
import shutil
import tempfile
import pexpect
from FileList import FileList

scriptDirName = os.path.dirname(os.path.abspath(os.path.realpath(__file__)))
sysImagesDirName = os.path.join(scriptDirName, 'system-images')

class SimhDriver:
    '''A base class for running and interacting with the simh system
       simulator as a child process.'''

    def __init__(self, simhCmd=None, cwd=None, debugStream=None, timeout=30):
        self.simhCmd = simhCmd if simhCmd is not None else "pdp11"
        self.debugStream = debugStream
        self.simh = None
        self.cwd = cwd
        self.timeout = timeout
        pass

    def start(self):
        if self.debugStream is not None:
            print('Starting simh: %s' % self.simhCmd, file=self.debugStream)
        echo = True if self.debugStream is not None else False
        self.simh = pexpect.spawn(self.simhCmd, cwd=self.cwd, echo=echo, encoding='utf-8', timeout=self.timeout)
        self.simh.logfile_read = self.debugStream
        self.simh.logfile_send = self.debugStream
        pass

    def stop(self):
        if self.simh is not None:
            if self.simh.isalive():
                self.simh.sendline('\x05')
                self.simh.expect_exact('sim> ')
                self.simh.sendline('q')
                self.simh.read()
                if self.debugStream is not None:
                    print('Waiting for simh to exit', file=self.debugStream)
                self.simh.wait()
                if self.debugStream is not None:
                    print('simh exited', file=self.debugStream)
            self.simh.close(force=False)
            self.simh = None

    def sendSimhCommands(self, script):
        self.simh.setecho(False)
        self.simh.sendline('')
        self.simh.expect_exact('sim> ', timeout=5)
        for line in script.split('\n'):
            line = line.strip()
            if len(line) > 0:
                self.simh.sendline(line)
                self.simh.expect_exact('sim> ')
        self.simh.setecho(True)

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()

class V6SimhDriver(SimhDriver):
    '''Runs a simulated v6 Unix system using simh'''

    defaultSystemDiskImage = os.path.join(sysImagesDirName, 'v6-test-system-rk05.dsk')

    def __init__(self, simhCmd=None, cwd=None, systemDiskImage=None, testDiskImage=None, debugStream=None, timeout=30):
        super().__init__(simhCmd=simhCmd, cwd=cwd, debugStream=debugStream, timeout=timeout)
        self.systemDiskImage = systemDiskImage if systemDiskImage is not None else type(self).defaultSystemDiskImage
        self.testDiskImage = testDiskImage
        self.tempDir = None

    def start(self):
        if self.simh is not None:
            raise RuntimeError('Simulator already running')
        try:
            if self.cwd is None:
                self.tempDir = tempfile.TemporaryDirectory()
                self.cwd = self.tempDir.name
            shutil.copyfile(self.systemDiskImage, os.path.join(self.cwd, 'system.dsk'))
            super().start()
            initScript = V6SimhDriver._initScript
            if self.testDiskImage is not None:
                initScript += "attach rk1 %s\n" % os.path.abspath(self.testDiskImage)
            if self.debugStream:
                print('\nConfiguring simulator', file=self.debugStream)
            self.sendSimhCommands(initScript)
            if self.debugStream:
                print('\nBooting v6 Unix', file=self.debugStream)
            self.sendSimhCommands('boot rk0\n')
            if self.debugStream:
                print('', file=self.debugStream)
            self.simh.expect_exact("@")
            self.simh.sendline("unix")
            self.simh.expect_exact("login: ")
            self.simh.sendline("root")
            self.simh.expect_exact("# ")
            self.simh.sendline("stty -echo")
            self.simh.expect_exact("# ")
        except:
            self.stop()
            raise

    def stop(self):
        try:
            if self.simh is not None:
                self.simh.sendline('')
                self.simh.expect_exact('# ', timeout=5)
                self.simh.sendline('sync; sync; sync')
                self.simh.expect_exact('# ', timeout=5)
        finally:
            try:
                super().stop()
            finally:
                try:
                    os.remove(os.path.join(self.cwd, 'system.dsk'))
                except:
                    pass
                if self.tempDir is not None:
                    self.tempDir.cleanup()
                    self.tempDir = None
                    self.cwd = None

    def sendShellCommands(self, script):
        res = ''
        self.simh.sendline('')
        self.simh.expect_exact('# ', timeout=5)
        for line in script.split('\n'):
            line = line.strip()
            if len(line) > 0:
                self.simh.sendline(line)
                self.simh.expect_exact('# ')
                res += self.simh.before
        return res

    def enumFiles(self, dir):
        fileListCmd = V6SimhDriver._fileListCmd % dir
        fileListStr = self.sendShellCommands(fileListCmd)
        return FileList.parse(fileListStr, V6SimhDriver._fileListRegex)

    def icheckFS(self, dev):
        # Invoke the V6 icheck program on the specified device.
        # Parse out various stats about the filesystems, as well
        # as any errors.
        icheckOut = self.sendShellCommands('/bin/icheck %s' % dev)
        m = re.search(string=icheckOut, pattern=r'^files\s+(\d+)\s*$', flags=re.M)
        fileCount = int(m.group(1)) if m else None
        m = re.search(string=icheckOut, pattern=r'^direc\s+(\d+)\s*$', flags=re.M)
        dirCount = int(m.group(1)) if m else None
        m = re.search(string=icheckOut, pattern=r'^spcl\s+(\d+)\s*$', flags=re.M)
        devCount = int(m.group(1)) if m else None
        m = re.search(string=icheckOut,  pattern=r'^used\s+(\d+)\s*$', flags=re.M)
        usedBlockCount = int(m.group(1)) if m else None
        m = re.search(string=icheckOut, pattern=r'^free\s+(\d+)\s*$', flags=re.M)
        freeBlockCount = int(m.group(1)) if m else None
        errs = list(filter(lambda line : 'bad' in line or 'missing' in line or 'dup' in line,
                    icheckOut.split('\n')))
        if len(errs) == 0:
            errs = None
        return (fileCount, dirCount, devCount, usedBlockCount, freeBlockCount, errs)

    _initScript = \
"""
set cpu 11/34 256k
set tti 7b
set tto 7b
set rk enabled
attach rk0 system.dsk
"""

    # v6 unix command to enumerate all entries in a given directory, printing their metadata
    # and, for files, a checksum of their contents.
    #
    # Note that this depends on a POSIX-compatible cksum command having been installed in
    # the test system image.  The source for this command is located in ancient-src/v6/extra.
    #
    _fileListCmd = '/usr/bin/find %s -exec /bin/ls -ild {} \\; -o \\( -type -f -a -exec /usr/bin/cksum {} \\; \\) -o \\( \\! -type -f -a -exec echo - \\; \\)'

    # Regex pattern to parse output from the above command.
    _fileListPattern = r'''^
                        \s* (?P<inode> \d+)
                        \s+ (?P<type> [bcd-]) (?P<mode> [rwsStTx-]{9}t?)
                        \s+ (?P<linkCount> \d+)
                        \s+ (?P<user> \w+)
                        \s+ (
                            ( (?P<size> \d+) ) |
                            ( (?P<major> \d+) \s* , \s* (?P<minor> \d+) )
                        )
                        \s+ (?P<time> [A-Za-z]{3} \s+ \d+ \s+ [0-9:]+ )
                        \s+ (?P<name> [^\n\r]+ ) [\r\n]+
                        \s* (
                            ( (?P<cksum> \d+ ) \s+ \d+ \s+ [^\n\r]+ ) |
                            -
                        )
                        \s*
                        $
                        '''
    _fileListRegex = re.compile(_fileListPattern, re.X|re.MULTILINE)

class V7SimhDriver(SimhDriver):
    '''Runs a simulated v7 Unix system using simh'''

    defaultSystemDiskImage = os.path.join(sysImagesDirName, 'v7-test-system-rp04.dsk')

    def __init__(self, simhCmd=None, cwd=None, systemDiskImage=None, testDiskImage=None, debugStream=None, timeout=30):
        super().__init__(simhCmd=simhCmd, cwd=cwd, debugStream=debugStream, timeout=timeout)
        self.systemDiskImage = systemDiskImage if systemDiskImage is not None else type(self).defaultSystemDiskImage
        self.testDiskImage = testDiskImage
        self.tempDir = None

    def start(self):
        if self.simh is not None:
            raise RuntimeError('Simulator already running')
        try:
            if self.cwd is None:
                self.tempDir = tempfile.TemporaryDirectory()
                self.cwd = self.tempDir.name
            shutil.copyfile(self.systemDiskImage, os.path.join(self.cwd, 'system.dsk'))
            super().start()
            initScript = self._initScript
            if self.testDiskImage is not None:
                initScript += "set rp1 enable\nset rp1 rp04\nattach rp1 %s\n" % os.path.abspath(self.testDiskImage)
            if self.debugStream:
                print('\nConfiguring simulator', file=self.debugStream)
            self.sendSimhCommands(initScript)
            if self.debugStream:
                print('\nBooting v7 Unix', file=self.debugStream)
            self.sendSimhCommands('boot rp0\n')
            if self.debugStream:
                print('', file=self.debugStream)
            self.simh.sendline("boot")
            self.simh.expect_exact(": ")
            self.simh.sendline("hp(0,0)unix")
            self.simh.expect_exact("# ")
            self.simh.sendline("stty -echo -lcase nl0 cr0")
            self.simh.expect_exact("# ")
        except:
            self.stop()
            raise

    def stop(self):
        try:
            if self.simh is not None:
                self.simh.sendline('')
                self.simh.expect_exact('# ', timeout=5)
                self.simh.sendline('sync; sync; sync')
                self.simh.expect_exact('# ', timeout=5)
        finally:
            try:
                super().stop()
            finally:
                try:
                    os.remove(os.path.join(self.cwd, 'system.dsk'))
                except:
                    pass
                if self.tempDir is not None:
                    self.tempDir.cleanup()
                    self.tempDir = None
                    self.cwd = None

    def sendShellCommands(self, script):
        res = ''
        self.simh.sendline('')
        self.simh.expect_exact('# ', timeout=5)
        for line in script.split('\n'):
            line = line.strip()
            if len(line) > 0:
                self.simh.sendline(line)
                self.simh.expect_exact('# ')
                res += self.simh.before
        return res

    def enumFiles(self, dir):
        fileListCmd = self._fileListCmd % dir
        fileListStr = self.sendShellCommands(fileListCmd)
        return FileList.parse(fileListStr, self._fileListRegex)

    def icheckFS(self, dev):
        # Invoke the V7 icheck program on the specified device.
        # Parse out various stats about the filesystems, as well
        # as any errors.
        icheckOut = self.sendShellCommands('/bin/icheck %s' % dev)
        m = re.search(string=icheckOut, pattern=r'^files\s+(\d+)\s+\(r=(\d+),d=(\d+),b=(\d+),c=(\d+)\)\s*$', flags=re.M)
        fileCount = int(m.group(2)) if m else None
        dirCount = int(m.group(3)) if m else None
        blockDevCount = int(m.group(4)) if m else None
        charDevCount = int(m.group(4)) if m else None
        m = re.search(string=icheckOut,  pattern=r'^used\s+(\d+)\s+\(i=\d+,ii=\d+,iii=\d+,d=\d+\)\s*$', flags=re.M)
        usedBlockCount = int(m.group(1)) if m else None
        m = re.search(string=icheckOut, pattern=r'^free\s+(\d+)\s*$', flags=re.M)
        freeBlockCount = int(m.group(1)) if m else None
        errRE = re.compile(r'(bad)|(dup)|(\d+\s+missing)', re.IGNORECASE)
        errs = [ line.strip() for line in icheckOut.split('\n') if errRE.search(line) ]
        if len(errs) == 0:
            errs = None
        return (fileCount, dirCount, blockDevCount, charDevCount, usedBlockCount, freeBlockCount, errs)

    _initScript = \
"""
set cpu 11/70
set cpu 2M
set cpu idle
set rp0 enable
set rp0 rp04
attach rp0 system.dsk
"""

    # v7 unix command to enumerate all entries in a given directory, printing their metadata
    # and, for files, a checksum of their contents.
    #
    # Note that this depends on a POSIX-compatible cksum command having been installed in
    # the test system image.  The source for this command is located in ancient-src/v7/extra.
    #
    _fileListCmd = '/bin/find %s -exec /bin/ls -ild {} \\; -a \\( -type f -a -exec /bin/cksum {} \\; \\) -o \\( \\! -type f -a -exec echo - \\; \\)'

    # Regex pattern to parse output from the above command.
    _fileListPattern = r'''^
                        \s* (?P<inode> \d+)
                        \s+ (?P<type> [bcd-]) (?P<mode> [rwsStTx-]{9}t?)
                        \s* (?P<linkCount> \d+)
                        \s+ (?P<user> \w+)
                        \s+ (
                            ( (?P<size> \d+) ) |
                            ( (?P<major> \d+) \s* , \s* (?P<minor> \d+) )
                        )
                        \s+ (?P<time> [A-Za-z]{3} \s+ \d+ \s+ [0-9:]+ )
                        \s+ (?P<name> [^\n\r]+ ) [\r\n]+
                        \s* (
                            ( (?P<cksum> \d+ ) \s+ \d+ \s+ [^\n\r]+ ) |
                            -
                        )
                        \s*
                        $
                        '''
    _fileListRegex = re.compile(_fileListPattern, re.X|re.MULTILINE)

class BSD29SimhDriver(SimhDriver):
    '''Runs a simulated 2.9BSD Unix system using simh'''

    defaultSystemDiskImage = os.path.join(sysImagesDirName, 'bsd29-test-system-rl02.dsk')

    def __init__(self, simhCmd=None, cwd=None, systemDiskImage=None, testDiskImage=None, debugStream=None, timeout=30):
        super().__init__(simhCmd=simhCmd, cwd=cwd, debugStream=debugStream, timeout=timeout)
        self.systemDiskImage = systemDiskImage if systemDiskImage is not None else type(self).defaultSystemDiskImage
        self.testDiskImage = testDiskImage
        self.tempDir = None

    def start(self):
        if self.simh is not None:
            raise RuntimeError('Simulator already running')
        try:
            if self.cwd is None:
                self.tempDir = tempfile.TemporaryDirectory()
                self.cwd = self.tempDir.name
            shutil.copyfile(self.systemDiskImage, os.path.join(self.cwd, 'system.dsk'))
            with open(os.path.join(self.cwd, 'swap.dsk'), "w+") as f:
                pass
            super().start()
            initScript = BSD29SimhDriver._initScript
            if self.testDiskImage is not None:
                initScript += "set rl2 rl02\nattach rl2 %s\n" % os.path.abspath(self.testDiskImage)
            if self.debugStream:
                print('\nConfiguring simulator', file=self.debugStream)
            self.sendSimhCommands(initScript)
            if self.debugStream:
                print('\nBooting 2.9BSD', file=self.debugStream)
            self.sendSimhCommands('boot rl0\n')
            if self.debugStream:
                print('', file=self.debugStream)
            self.simh.expect_exact(": ")
            self.simh.send("rl(0,0)unix\r")
            self.simh.expect_exact("# ")
            self.simh.sendline("stty -echo")
            self.simh.expect_exact("# ")
        except:
            self.stop()
            raise

    def stop(self):
        try:
            if self.simh is not None:
                self.simh.sendline('')
                self.simh.expect_exact('# ', timeout=5)
                self.simh.sendline('sync; sync; sync')
                self.simh.expect_exact('# ', timeout=5)
        finally:
            try:
                super().stop()
            finally:
                try:
                    os.remove(os.path.join(self.cwd, 'system.dsk'))
                except:
                    pass
                try:
                    os.remove(os.path.join(self.cwd, 'swap.dsk'))
                except:
                    pass
                if self.tempDir is not None:
                    self.tempDir.cleanup()
                    self.tempDir = None
                    self.cwd = None

    def sendShellCommands(self, script) -> str:
        res = ''
        self.simh.sendline('')
        self.simh.expect_exact('# ', timeout=5)
        for line in script.split('\n'):
            line = line.strip()
            if len(line) > 0:
                self.simh.sendline(line)
                self.simh.expect_exact('# ')
                res += self.simh.before
        return res

    def enumFiles(self, dir):
        fileListCmd = BSD29SimhDriver._fileListCmd % dir
        fileListStr = self.sendShellCommands(fileListCmd)
        return FileList.parse(fileListStr, BSD29SimhDriver._fileListRegex)

    def fsck(self, dev):
        # Invoke the 2.9BSD fsck program on the specified device
        fsckOut = self.sendShellCommands('/etc/fsck -n %s' % dev)

        m = BSD29SimhDriver._fsckCheckingRegex.search(fsckOut)
        if not m:
            raise RuntimeError('Unexpected output from fsck')
        resultsStart = m.end()

        m = BSD29SimhDriver._fsckSummaryRegex.search(fsckOut)
        if not m:
            raise RuntimeError('Unexpected output from fsck')
        resultsEnd = m.start()

        results = fsckOut[resultsStart:resultsEnd]

        files = int(m.group('files'))
        blocks = int(m.group('blocks'))
        free = int(m.group('free'))

        if results == BSD29SimhDriver._fsckNoErrors:
            results = None

        return (files, blocks, free, results)

    _initScript = """\
set cpu 11/23+
set cpu 256K
set cpu nocis
set cpu fpp
set tti 7b
set tto 7b
set rl enabled
set rl0 rl02
attach rl0 system.dsk
set rl1 rl02
attach rl1 swap.dsk
"""

    # 2.9BSD command to enumerate all entries in a given directory, printing their metadata
    # and, for files, a sum of their contents.
    _fileListCmd = '/bin/find %s -exec /usr/ucb/ls -ildn {} \\; \\( -type f -exec /bin/cksum {} \\; \\) -o -exec echo - \\;'

    # Regex pattern to parse output from the above command.
    _fileListPattern = r'''^
                        \s* (?P<inode> \d+)
                        \s+ (?P<type> [bcd-]) (?P<mode> [rwsStTx-]{9})
                        \s* (?P<linkCount> \d+)
                        \s+ (?P<uid> \d+)
                        \s+ (?P<gid> \d+)
                        \s+ (
                            ( (?P<size> \d+) ) |
                            ( (?P<major> \d+) \s* , \s* (?P<minor> \d+) )
                        )
                        \s+ (?P<time> [A-Za-z]{3} \s+ \d+ \s+ [0-9:]+ )
                        \s+ (?P<name> [^\n\r]+ ) [\r\n]+
                        \s* (
                            ( (?P<cksum> \d+ ) \s+ \d+ \s+ [^\n\r]+ ) |
                            -
                        )
                        \s*
                        $
                        '''
    _fileListRegex = re.compile(_fileListPattern, re.X|re.MULTILINE)

    _fsckCheckingPattern = r'^\*\*\s+Checking\s+.*\n';
    _fsckCheckingRegex = re.compile(_fsckCheckingPattern, re.MULTILINE)
    _fsckSummaryPattern = r'^\s*(?P<files>\d+)\s+files\s+(?P<blocks>\d+)\s+blocks\s+(?P<free>\d+)\s+free\s*\n'
    _fsckSummaryRegex = re.compile(_fsckSummaryPattern, re.MULTILINE)
    _fsckNoErrors = '''\
** Phase 1 - Check Blocks and Sizes\r
** Phase 2 - Check Pathnames\r
** Phase 3 - Check Connectivity\r
** Phase 4 - Check Reference Counts\r
** Phase 5 - Check Free List \r
'''
