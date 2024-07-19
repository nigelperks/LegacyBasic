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


# typ is both reference suffix ("fred.out") and actual output ("out")
def check_output(source, typ, update):
  if not source.endswith(".bas"):
    fatal(".bas expected: " + source)
  ref = source[:-3] + typ
  what = "ERROR" if typ == "err" else "OUTPUT"
  if os.path.isfile(ref):
    if os.path.isfile(typ):
      if not filecmp.cmp(ref, typ):
        print("ERROR: different output was expected")
        offer_new_reference(ref, typ, what, update)
    else:
      fatal("Output file expected: " + typ)
  else:
    if os.path.isfile(typ):
      with open(typ) as f:
        out = f.read()
      if len(out) > 0:
        offer_new_reference(ref, typ, what, update)


def print_file(name):
  if os.path.isfile(name):
    with open(name) as f:
      line = f.readline()
      while line != "":
        print(line,end="")
        line = f.readline()

def offer_new_reference(ref, typ, what, update):
  print("--- EXPECTED " + what + " ----")
  print_file(ref)
  print("---- ACTUAL " + what + " -----")
  print_file(typ)
  print("-----------------" + "-" * len(what))
  if update:
    print("Make new reference " + ref + " ?")
    if input().startswith("y"):
      if os.path.isfile(ref):
        os.remove(ref)
      check_cmd("copy " + typ + " " + ref)
      return
    print("New reference not created")
  sys.exit(1)


def test(exe, source, update, SyntaxOnly, BasicOptions):
# banner(source)
  options = " ".join(BasicOptions)
  if SyntaxOnly:
    options += " --parse "
  with open(source) as f:
    top = f.readline()
    if "REM OPTION " in top:
      options = top.split("REM OPTION ")[1].strip()
  cmd = exe + " -q " + options + " " + source
  if not SyntaxOnly:
    cmd += " >out 2>err"
  print(">", cmd)
  check_cmd(cmd)
  if not SyntaxOnly:
    check_output(source, "out", update)
    check_output(source, "err", update)
    print("PASS")
  return 1


# MAIN

TestsDir = "tests"
BinaryName = "LegacyBasic"
BinaryDir = None
TestNames = []
update = False
SyntaxOnly = False
BasicOptions = []

i = 1
while i < len(sys.argv):
  arg = sys.argv[i]
  if arg in ["--help","-h","-?","/?"]:
    print("Usage:   test.py --tests=TESTS-DIR --dir=EXECUTABLE-DIR --bin=EXECUTABLE-NAME")
    print("Example: test.py --tests=tests --dir=Debug --bin=LegacyBasic.exe")
    sys.exit(1)
  if arg[0] == '-':
    if arg.startswith("--tests="):
      TestsDir = arg[len("--tests="):]
    elif arg.startswith("--bin=") or arg.startswith("--exe="):
      BinaryName = arg[6:]
    elif arg.startswith("--dir="):
      BinaryDir = arg[6:]
    elif arg == "--syntax":
      SyntaxOnly = True
    elif arg == "-u" or arg == "--update":
      update = True
    elif arg == "-b" or arg == "--basic":
      i += 1
      if i >= len(sys.argv):
        fatal("Basic option missing")
      BasicOptions.append(sys.argv[i])
    else:
      fatal("test.py unknown option: " + arg)
  else:
    TestNames.append(arg)
  i += 1

if os.name == "nt":
  if not BinaryName.endswith(".exe"):
    BinaryName += ".exe"

print("TestsDir", TestsDir)
print("Binary", BinaryName)
print("BuildDir", BinaryDir)
print("Names", TestNames)
print()

if not os.path.isdir(TestsDir):
  fatal("Tests directory not found: " + TestsDir)

AllTests = glob(os.path.join(TestsDir, "*.bas"))

if BinaryDir is None:
  BinaryDir = ""

ExeFound = 0
Passes = 0

for sub in ["", "Release", "Debug"]:
  exe = os.path.abspath(os.path.join(BinaryDir, sub, BinaryName))
  print("Looking for executable: " + exe)
  if os.path.isfile(exe):
    ExeFound += 1
    banner(exe)
    if TestNames != []:
      for source in TestNames:
        if os.path.isdir(source):
          names = glob(os.path.join(source, "*.bas"))
          for name in names:
            Passes += test(exe, name, update, SyntaxOnly, BasicOptions)
        if os.path.isfile(source):
          Passes += test(exe, source, update, SyntaxOnly, BasicOptions)
        if os.path.isfile(os.path.join(TestsDir, source)):
          Passes += test(exe, os.path.join(TestsDir, source), update, SyntaxOnly, BasicOptions)
        bas = source + ".bas"
        if os.path.isfile(bas):
          Passes += test(exe, bas, update, SyntaxOnly, BasicOptions)
        if os.path.isfile(os.path.join(TestsDir, bas)):
          Passes += test(exe, os.path.join(TestsDir, bas), update, SyntaxOnly, BasicOptions)
    else:
      for source in AllTests:
        Passes += test(exe, source, update, SyntaxOnly, BasicOptions)

if ExeFound == 0:
  fatal("No Legacy Basic interpreter found")

banner("Summary")
print("Pass:", Passes)
print("Fail:", 0)
sys.exit(0)
