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


def offer_new_reference(ref, typ, update=False):
  print("--------")
  with open(typ) as f:
    line = f.readline()
    while line != "":
      print(line,end="")
      line = f.readline()
  print("--------")
  if update:
    print("Make new reference?")
    if input().startswith("y"):
      if os.path.isfile(ref):
        os.remove(ref)
      check_cmd("copy " + typ + " " + ref)
      return
  sys.exit(1)


def test(exe, source):
# banner(source)
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

TestsDir = "tests"
BinaryName = "LegacyBasic"
BinaryDir = None

for arg in sys.argv[1:]:
  if arg.startswith("--tests="):
    TestsDir = arg[len("--tests="):]
  elif arg.startswith("--bin=") or arg.startswith("--exe="):
    BinaryName = arg[6:]
  elif arg.startswith("--dir="):
    BinaryDir = arg[6:]
  else:
    fatal("unrecognised argument: " + arg)

if os.name == "nt":
  if not BinaryName.endswith(".exe"):
    BinaryName += ".exe"

print("TestsDir", TestsDir)
print("Binary", BinaryName)
print("BuildDir", BinaryDir)
print()

if not os.path.isdir(TestsDir):
  fatal("Tests directory not found: " + TestsDir)

Tests = glob(os.path.join(TestsDir, "*.bas"))

if BinaryDir is None:
  BinaryDir = ""

ExeFound = 0
Passes = 0

for sub in ["", "Release", "Debug"]:
  exe = os.path.abspath(os.path.join(BinaryDir, sub, BinaryName))
  if os.path.isfile(exe):
    ExeFound += 1
    banner(exe)
    for source in Tests:
      test(exe, source)
      Passes += 1

if ExeFound == 0:
  fatal("No Legacy Basic interpreter found")

banner("Summary")
print("Pass:", Passes)
print("Fail:", 0)
sys.exit(0)
