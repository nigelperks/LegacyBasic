#!/usr/bin/python3

import os
import sys
from glob import glob
import filecmp

def fatal(msg):
  print("FATAL:", msg)
  sys.exit(1)

def banner(s):
  b = "*" * (len(s) + 4)
  print(b)
  print("*", s, "*")
  print(b)

def check_cmd(cmd):
  r = os.system(cmd)
  if r != 0:
    print("Command failed: exit code", r)
    sys.exit(1)

def check_output(source, typ):
  if not source.endswith(".bas"):
    fatal(".bas expected: " + source)
  ref = source[:-3] + typ
  if os.path.isfile(ref):
    if os.path.isfile(typ):
      if not filecmp.cmp(ref, typ):
        print("ERROR: different output was expected")
        offer_new_reference(ref, typ)
    else:
      fatal("Output file expected: " + typ)
  else:
    if os.path.isfile(typ):
      with open(typ) as f:
        out = f.read()
      if len(out) > 0:
        print("ERROR: no output was expected")
        offer_new_reference(ref, typ)

def offer_new_reference(ref, typ):
        print("--------")
        with open(typ) as f:
          line = f.readline()
          while line != "":
            print(line,end="")
            line = f.readline()
        print("--------")
        print("Make new reference?")
        if input().startswith("y"):
          if os.path.isfile(ref):
            os.remove(ref)
          check_cmd("copy " + typ + " " + ref)
        else:
          sys.exit(1)

def test(exe, source):
  banner(source)
  options = ""
  with open(source) as f:
    top = f.readline()
    if "REM OPTION " in top:
      options = top.split("REM OPTION ")[1].strip()
  cmd = exe + " -q " + options + " " + source + " >out 2>err"
  print(">", cmd)
  check_cmd(cmd)
  check_output(source, "out")
  check_output(source, "err")
  print("PASS")

# MAIN

if len(sys.argv) != 2:
  fatal("Usage: test.py legacy-basic.exe")
exe = sys.argv[1]
if not os.path.isfile(exe):
  fatal("Basic interpreter not found: " + exe)
exe = os.path.abspath(exe)

if not os.path.isdir("tests"):
  fatal("tests directory not found")
os.chdir("tests")
count = 0
for source in glob("*.bas"):
  test(exe, source)
  count += 1
banner("Summary")
print("Pass:", count)
print("Fail:", 0)
