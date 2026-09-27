#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from subprocess import Popen, PIPE
import platform
import os

def pipe_through_prog(argv, text):
    p1 = Popen(argv, stdout=PIPE, stdin=PIPE, stderr=PIPE)
    [result, err] = p1.communicate(input=text.encode('utf-8'))
    result = result.decode('utf-8')
    # On Windows the program's stdout is in text mode, so every "\n" is
    # written as "\r\n". Undo that so the output compares against the
    # LF-only fixtures.
    if platform.system() == 'Windows':
        result = result.replace('\r\n', '\n')
    return [p1.returncode, result, err]

class Prog:
    def __init__(self, cmdline="org2html", default_options=[]):
        # The program path itself may contain spaces.
        if os.path.exists(cmdline) or os.path.exists(cmdline + ".exe"):
            self.cmdline = [cmdline]
        else:
            self.cmdline = cmdline.split()
        if len(self.cmdline) <= 1:
            # cmdline provided no command line options. Use default ones.
            if isinstance(default_options, str):
                self.cmdline += default_options.split()
            else:
                self.cmdline += default_options
        self.to_html = lambda x: pipe_through_prog(self.cmdline, x)
