# LEGACY BASIC

Legacy Basic is an interpreter for the legacy of 1970s-style microcomputer BASIC
programs, especially text-based games.

Please see LICENSE.

1. Installer for Windows
2. Building the interpreter from source
3. Running a BASIC program in a file
4. Immediate mode
5. References
6. Technical details
7. Contributions
8. Bug reports


## 1. INSTALLER FOR WINDOWS

To run on Linux, or if the installer does not work, build from source as below.

I hope the installer is useful, but it has not been widely tested for
compatibility.

Run the NSIS installer:

```
LegacyBasic-3.1.0-win64.exe
```

The executable is installed to `bin`, documents to `doc`.


## 2. BUILDING THE INTERPRETER FROM SOURCE

CMake will generate makefiles and project files for both Windows and Linux.

First change to the source directory containing `CMakeLists.txt`.

### Building on Windows

Using Visual Studio 17 2022 on 64-bit Windows for example. This is a
multi-configuration generator. The configuration (Debug, Release) is selected
at build time.

```
md Build
cmake -S . -B Build -G "Visual Studio 17 2022" -A x64 -T host=x64
cmake --build Build --config Release
```

If Python is not installed, so that tests cannot be run, add `-DLBASIC_NOTEST=1`
to the generation command.

#### Testing

```
ctest -C Release --test-dir Build
```

#### Installing

To install in `C:\Local\bin` for example:

```
cmake --install Build --config Release --prefix C:\Local
```

### Building on Linux

Using makefiles for example. This is a single-configuration generator. The
configuration (Debug, Release) is selected at generation time.

```
mkdir Release
cmake -S . -B Release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build Release
```

#### Testing

```
ctest -C Release --test-dir Release
```

#### Installing

To install in `/usr/local/bin` for example:

```
cmake --install Release --prefix /usr/local
```


## 3. RUNNING A BASIC PROGRAM IN A FILE

To run a BASIC program in a file:

    LegacyBasic  game.bas        (Windows)
    legacy-basic game.bas        (Linux)

To use BASIC interactively (immediate mode), run:

    LegacyBasic                  (Windows)
    legacy-basic                 (Linux)

with no BASIC file name.

I found target programs at:

- http://www.vintage-basic.net/
- http://www.dunnington.info/public/basicgames

To play a game which uses random numbers, use the --randomize option to get
different behaviour each time you play:

    LegacyBasic hangman.bas --randomize

To run a program in which the keywords are crunched together, for example
LETA=BANDC meaning LET A = B AND C, use the --keywords-anywhere option.

To get full help on all the options, use --help-full.


## 4. IMMEDIATE MODE (INTERACTIVE)

When LegacyBasic is run with no BASIC source file specified, immediate mode is
entered.

Use BYE to return to the operating system.

Enter a program line interactively by typing a line number:

    10 PRINT "HELLO! ";:GOTO 10

There is no EDIT command or line editing: retype a line to replace it.

Type a line number followed immediately by ENTER to delete that line.

Most BASIC statements can be used interactively. Definitions of variables,
arrays and DEF functions are shared between program and interactive environment.
So a definition made in the program is available after it is run, in the
interactive environment. And a definition made interactively is available in the
program, if a program segment is entered with GOTO. However, RUN clears all
definitions, as does CLEAR.

If any change is made to the source program, it is recompiled when a statement
is next executed, and the state of GOSUB/RETURN and FOR loops is cleared. Thus a
GOTO or NEXT might continue a FOR loop in progress, but not after a program edit.

The state of GOSUB/RETURN and FOR loops is also cleared if it would refer to an
immediate statement that no longer exists.

When code is running in interactive mode, CTRL-C should return to the BASIC
prompt, rather than exiting the interpreter (tested on Windows and Ubuntu).

Immediate mode commands are:

    BYE
    LIST [start-end]
    LOAD "prog.bas"
    NEW
    RUN
    SAVE "prog.bas"


### Example 1

    > a$="Hello!"
    > ?a$
    Hello!
    > dim k(27)
    > input k(13):print k(13)
    ? 12.3
     12.3
    > for cent = 0 to 100 step 10:print cent, int((cent*9/5)+32):next
     0       32
    ...
    > def double(x)=2*x
    > ?double(1.3),double(-3)
     2.6     -6
    > bye


### Example 2

    > 10 FOR i = 1 TO 4
    > 20 PRINT i
    > 30 IF i = 2 THEN STOP
    > 40 NEXT
    > RUN
     1
     2
    Stopped
    > NEXT
     3
     4


### Example 3

    > 10 PRINT double(5.1)
    > DEF double(x)=2*x
    > GOTO 10
     10.2


## 5. REFERENCES

John G. KEMENY and Thomas E. KURTZ, "Back to BASIC", 1985.

    The inventors of BASIC narrate its origin at Dartmouth College and argue
    that the interpreted BASICs went horribly wrong and mauled the language.
    Eye-opening.

"Birth of Basic", https://www.youtube.com/watch?v=WYPNjSoDrqw, 2014
(accessed 2022-03-05).

    Wonderful documentary on Kemeny and Kurtz's creation of BASIC.

Microsoft, "GW-BASIC Ver.3.2 User's Guide", 1986.

David H. Ahl, "Basic Computer Games: Microcomputer Edition", 1978.

David H. Ahl, "More Basic Computer Games", 1979.


## 6. TECHNICAL DETAILS

Legacy Basic parses the source file from beginning to end, translating it into
an intermediate code. So any syntax errors or unsupported constructs are found
up front, not in the middle of a run. The intermediate code targets a
stack-based virtual machine. It has operators to handle run-time definition of
arrays and functions, and FOR loops which break static nesting.

I emphasised informative error messages at both parse and run time.


## 7. CONTRIBUTIONS

Original Linux support (before CMake) by yuppox.


## 8. BUG REPORTS

Bug reports are welcome to the email address in the git history.

Nigel Perks, 2022-3.
