LEGACY BASIC
============

Legacy Basic is an interpreter for the legacy of 1970s-style microcomputer BASIC
programs, especially text-based games.

To use:

    LegacyBasic.exe game.bas

where game.bas is the BASIC source file.

I found target programs at:

- http://www.vintage-basic.net/
- http://www.dunnington.info/public/basicgames

To play a game which uses random numbers, use the --randomize option to get
different behaviour each time you play:

    LegacyBasic.exe hangman.bas --randomize

To run a program in which the keywords are crunched together, for example
LETA=BANDC meaning LET A = B AND C, use the --keywords-anywhere option.

To get full help on all the options, use --help-full.

REFERENCES
----------
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

TECHNICAL DETAILS
-----------------
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

CONTRIBUTIONS
-------------
Linux support by yuppox.

Nigel Perks, 2022.
