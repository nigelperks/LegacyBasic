# LEGACY BASIC

Legacy Basic is an interpreter for the legacy of 1970s-style microcomputer BASIC
programs, especially text-based games.

To run a BASIC program in a file:

    LegacyBasic.exe game.bas       (Windows)
    legacyBasic game.bas           (Linux)

To use BASIC interactively (immediate mode), run:

    LegacyBasic.exe                (Windows)
    legacyBasic                    (Linux)

with no BASIC file name.

I found target programs at:

- http://www.vintage-basic.net/
- http://www.dunnington.info/public/basicgames

To play a game which uses random numbers, use the --randomize option to get
different behaviour each time you play:

    LegacyBasic.exe hangman.bas --randomize

To run a program in which the keywords are crunched together, for example
LETA=BANDC meaning LET A = B AND C, use the --keywords-anywhere option.

To get full help on all the options, use --help-full.

## IMMEDIATE MODE (INTERACTIVE)

When LegacyBasic is run with no BASIC source file specified,
immediate mode is entered.
Most BASIC statements can be used interactively.
Assignments to variables and arrays are persistent.
DEF definitions last only within one command line: that is, as long as their code exists.
Use BYE to return to the operating system.

When code is running in interactive mode, CTRL-C should return to
the BASIC prompt, rather than exiting the interpreter (tested on Windows and Ubuntu).

Enter a program line interactively by typing a line number:

    10 PRINT "HELLO! ";:GOTO 10

There is no EDIT command or line editing: retype a line to replace it.

Immediate mode commands are:

    BYE
    LIST [start-end]
    LOAD "prog.bas"
    NEW
    RUN
    SAVE "prog.bas"


### Example

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
    > def double(x)=2*x:?double(1.3),double(-3)
     2.6     -6
    > bye


## REFERENCES

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

## TECHNICAL DETAILS

Legacy Basic parses the source file from beginning to end, translating it into
an intermediate code. So any syntax errors or unsupported constructs are found
up front, not in the middle of a run. The intermediate code targets a stack-
based virtual machine. It has operators to handle run-time definition of arrays
and functions, and FOR loops which break static nesting.

I emphasised informative error messages at both parse and run time.

To run unit tests (with CuTest) in a Debug build:

    LegacyBasic.exe -unittest

To run system tests on several test files:

    test.py x64\Debug\LegacyBasic.exe

specifying the executable interpreter to be tested.

## CONTRIBUTIONS

Original Linux support by yuppox.

## BUG REPORTS

Bug reports are welcome to the email address in the git history.

Nigel Perks, 2022.
