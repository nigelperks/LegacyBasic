
.. |copy| unicode:: U+00A9
.. |pi|   unicode:: U+03C0

Legacy Basic 3.2.0
##################

By Nigel Perks
**************

==================
Using Legacy Basic
==================

Legacy Basic was written to run our "legacy"
of interpreted microcomputer Basic games from the 1970s/80s.
Its distinguishing feature is intended to be that it includes
only those statements and features needed to run the target programs.
To install or build from source, see the README.
To run a Basic program on Windows, use::

  LegacyBasic game.bas

and on Linux, use::

  legacy-basic game.bas

To use immediate mode, run the interpreter without specifying a Basic file.

==============
Immediate mode
==============

Immediate mode is the mode of a Basic interpreter
in which you enter commands which are performed immediately,
as opposed to the mode in which the interpreter is running a program.

Legacy Basic enters immediate mode
when the interpreter is run with no Basic source file specified.
This can be used to give the feel of using the micro Basics,
and to check how a statement or function behaves.

In immediate mode there is a current source file, that is, a current Basic program.

Two types of activity can be performed in immediate mode:

1. Working with the current source file.
2. Running Basic statements immediately, outside a program.

Working with the current source file
------------------------------------
A line of text beginning with a number
is entered into the current source file, with that line number.
It replaces any existing line with that number.

A line number followed by ENTER, with no text,
deletes that line from the source file, if it exists.

These are the only means of changing the source file within Legacy Basic.
There is no EDIT command or screen editor.

Most of the following commands work with the current source file (current Basic program).

Immediate commands
------------------

BYE
^^^
Return to the operating system.

LIST
^^^^
Lists the current source file.
A starting line number, an ending line number, or a line number range may be given::

  LIST start – end 

The command might pause after each page of the listing: press ENTER to continue the listing.

LOAD
^^^^
Load a source file from disk.
Example: ``LOAD "prog.bas"``

NEW
^^^
Delete the current source file from memory and begin a new program. This does not affect disk files.

RUN
^^^
Run the current source file as a Basic program.

SAVE
^^^^
Save the current source file to disk under the given file name.
Example: ``SAVE "prog.bas"``

Immediate statements
--------------------
Most BASIC statements can be used interactively.
Definitions of variables, arrays and ``DEF`` functions
are shared between program and interactive environment.
So a definition made in the program is available after it is run,
in the interactive environment.
And a definition made interactively is available in the program,
if a program segment is entered with ``GOTO``.
However, ``RUN`` clears all definitions, as does ``CLEAR``.

If any change is made to the source program,
it is recompiled when a statement is next executed,
and the state of ``GOSUB``/``RETURN`` and ``FOR`` loops is cleared.
Thus a ``GOTO`` or ``NEXT`` might continue a ``FOR`` loop in progress,
but not after a program edit.

The state of ``GOSUB``/``RETURN`` and ``FOR`` loops is also cleared
if it would refer to an immediate statement that no longer exists.

When code is running in interactive mode,
CTRL-C should return to the Basic prompt,
rather than exiting the interpreter (tested on Windows and Ubuntu).

Example 1
^^^^^^^^^

::

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

Example 2
^^^^^^^^^

::

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

Example 3
^^^^^^^^^

::

  > 10 PRINT double(5.1)
  > DEF double(x)=2*x
  > GOTO 10
   10.2

==========
Statements
==========

A line may consist of multiple statements separated by colons.

CLEAR
-----
Delete all user-defined variables, arrays and functions.

CLS
---
Clear screen.

DATA
----
Declare numeric and string data to be read with READ::

  10 DATA “Quoted string”, 53.2, unquoted string

DEF
---
Define a function.

Traditionally, user-defined functions were named using ``FN`` and a single letter,
``FNA`` to ``FNZ``, but in Legacy Basic a name of any length can be used,
and ``FN`` has no special signficance.

The function may return a number or a string,
but must always take one numeric parameter,
which need not be used in the function value::

  DEF double(x) = 2 * x
  DEF pair$(x) = str$(x) + “,” + str$(x)
  DEF name$(z) = “Fred Smith”

Assigning a value to the parameter when a function is called
does not affect a program variable of that name::

  10 DEF double(x) = 2 * x
  20 x = 14
  30 PRINT x, double(100), x
  40 REM prints 14, 200, 14

DIM
---
Example::

  DIM a(4), a$(5), b(7,8)

Dimension arrays.
Both numeric and string arrays are supported, of one or two dimensions.
Subscripts range from 0 to the given number.
So ``a(4)`` has five elements, ``a(0)`` to ``a(4)``.
This means that 0 is a valid maximum subscript: ``DIM z(0)``.

If an array is referenced in an expression without having been dimensioned,
for example ``k(3)``,
then it is dimensioned on the fly.
The maximum index of each dimension is the given number or 10,
whichever is greater.
So ``k(3)`` would implicitly dimension ``k(10)``,
but ``k(18)`` would dimension ``k(18)``.

When an array dimensioned by ``DIM`` already exists, it is first deleted.
So all elements are cleared to zero or the empty string,
even if the dimensions are exactly the same as before.

END
---
End the run of the program. Unlike ``STOP``, ``END`` does not print anything.

FOR
---
Perform a loop for each value of an index variable in a given range::

  FOR i = 1 to 10:code:NEXT i

performs code for each value of ``i`` from 1 to 10 inclusive.

The variable must be a simple variable, not a subscripted array element.

The amount by which the variable is stepped up each time can be specified with ``STEP``.
So a downward loop can be performed using ``STEP -1`` or other negative step.
A step of 0 produces an infinite loop.

Note that the loop code is executed at least once,
even when the range seems to be empty,
for example::

  FOR i = 1 to 0

This is unusual for a compiled language, and does not match Dartmouth Basic,
but is a feature of the interpreted Basics emulated by Legacy Basic.

If the specified index variable is already the index of a loop being executed,
that loop is exited, and a new loop is begun.
The new loop becomes the innermost loop.
This behaviour too was chosen to execute the target programs correctly,
even though it seems unusual after using structured, compiled languages.

GOSUB
-----
Syntax::

  GOSUB line-number

Go to a subroutine: go to the given line number in the program,
returning to the current position on ``RETURN``.

If no line has that line number, a run-time error occurs.

There is a limit to the number of locations to ``RETURN`` to that can be stacked up.
If a ``GOSUB`` would exceed that limit,
run-time error ``GOSUB is nested too deeply`` occurs.

Example::

  10 GOSUB 100
  20 PRINT “Back from subroutine”
  30 END
  100 PRINT “I am a subroutine”
  110 RETURN

GOTO
----
Syntax::

  GOTO line-number

Go to the given line number in the program.

If no line has that line number, a run-time error occurs.

Example::

  10 GOTO 30
  20 PRINT “This will not be printed”
  30 END

IF
--
Three forms are supported::

  IF numeric-value THEN line-number [ELSE line-number]
  IF numeric-value THEN statements
  IF numeric-value THEN non-IF-statements ELSE statements

If the numeric value is non-zero (true),
go to the line number, or perform the statements, after ``THEN``.

If the numeric value is zero (false), and there is an ``ELSE clause``,
go to the line number, or perform the statements, after ``ELSE``.

If the numeric value is zero (false), and there is no ``ELSE`` clause,
go to the next program line.

INPUT
-----
Syntax::

  INPUT [string-constant [';' | ',']] item1 [',' item2 ...]

Input one or more values, numeric or string, into variables or array elements.

If a string constant is given, it is printed as a prompt.
A question mark is also always printed as a prompt.

Commas in the input separate the values to be assigned to the items.
So multiple values can be input at once, but a comma cannot be input in a string.
If insufficient comma-separated values are provided at run time,
Legacy Basic prompts::

  More input items are expected

and repeats the ``INPUT`` statement.

If the input line has a comma after all input items have been assigned,
that comma and anything following are discarded, and Legacy Basic reports::

  Extra input was discarded

If a numeric value is expected, and the input is not a valid number,
Legacy Basic reports::

  Invalid input

and the ``INPUT`` statement is repeated.

Example::

  INPUT “Name, age”; name$, age

See also ``LINE INPUT``.

LET
---
Assign the value of a numeric or string expression
to a numeric or string (respectively) variable or array element::

  [LET] variable-or-array-element '=' expression

The ``LET`` keyword is optional in an assignment.

LINE INPUT
----------
Syntax::

  LINE INPUT [string-constant [';' | ',']] string-variable

Input an entire line, including commas, into a string variable or array element.
If a string constant is given, it is printed as a prompt.
A question mark is also always printed as a prompt.

Example::

  LINE INPUT “Name”; name$(i)

NEXT
----
Perform the next iteration of a loop. Can be used with or without an index variable::

  NEXT
  NEXT i
  NEXT j, i

Without a variable,
``NEXT`` performs the next iteration of the loop most recently started with ``FOR``.
The innermost loop in interpreted Basic
is not necessarily the innermost loop in the structure of the code.

With a variable that is the index variable of a loop,
``NEXT`` performs the next iteration of that loop,
whether or not that loop is the most recently created one.

``NEXT`` with two or more variables is equivalent to consecutive ``NEXT`` in the same order,
so that ``NEXT j, i`` is equivalent to ``NEXT j:NEXT i``.
The innermost variable comes first.

ON
--
Syntax::

  ON numeric-value { GOTO | GOSUB } line1, line2, ...

Choose a line number or subroutine to go to, depending on a numeric value.
If the value is 1, the first line number is used;
if the value is 2, the second; and so on.
If the value is an integer less than 1 or greater than the number of line numbers,
execution falls through to the statement after ``ON``.
If the value is not an integer, a run-time error occurs.
For ``ON ... GOSUB``, on ``RETURN``, execution returns to the statement after ``ON``.

PRINT
-----
The question mark, ``?``, may be used instead of the ``PRINT`` keyword, for brevity.

Print values, and set print position, on screen;
more precisely, print to standard output.
Print numeric and string values: constants, variables, expressions.
A number is printed with a space before and after.

Items may optionally be separated by semicolon or comma.
A semicolon has no effect on the print position.
A comma moves the print position to the next 8-column field.

Operator ``SPC(n)`` prints n spaces.

Operator ``TAB(n)`` moves the print position to column ``n``,
where column 1 is the first column.

Example::

  PRINT “Data: “; x$; TAB(20); x, (x+7)*2

RANDOMIZE
---------
The built-in function ``RND``
returns a pseudo-random number between 0 and 1.
The number is "pseudo" random, not truly random, because it is computed.
Computing one random number after another produces a list of numbers.
The next number to be produced by ``RND`` is determined by the previous number produced,
or by an initial number if none have been produced yet.
The same list will be produced every time a program is run,
because it is computed from the same initial value.

``RANDOMIZE`` attempts to randomize the number generator
by changing the number to base the next computation on,
based on the current time of day.

``RANDOMIZE n``, where ``n`` is a non-negative integer,
causes the next number produced by ``RND`` to be based on number ``n``.
So::

  RANDOMIZE 100:A=RND:B=RND

will put the same numbers in ``A`` and ``B`` every time it is run.

READ
----
Read numeric or string data from the ``DATA`` list into a variable or array element::

  10 READ a$, a
  20 DATA “string item”, 3.14

It is a run-time error to read non-numeric data into a numeric variable or array element.

REM
---
Remark (comment)::

  REM text

The rest of the line is ignored by Legacy Basic.

Example::

  10 REM this is a great program
  20 PRINT “Hello”

RESTORE
-------
Syntax::

  RESTORE [line-number]

Restore the pointer from which to ``READ`` data,
either to the beginning of the program or to a specific line::

  500 RESTORE
  510 READ a$: REM reads from the first DATA in the program
  520 RESTORE 1000
  530 READ a$: REM reads from the first DATA on or after line 1000

RETURN
------
Syntax::

  RETURN

Return from a subroutine called with ``GOSUB``.
The program continues running after the ``GOSUB`` statement.

STOP
----
Syntax::

  STOP

Stop running the program.
Prints the program line containing ``STOP``, and the message::

  Stopped


==================
Built-in functions
==================

ABS
---
Returns the absolute value of a number.
This is the size of a number regardless of its sign.
For example ``ABS(3.2)`` and ``ABS(-3.2)`` both equal 3.2.

ASC
---
Returns the ASCII value of the given character,
for example ``ASC("A")`` is 65.
Returns 0 when given an empty string.

ATN
---
Returns the arctangent of the given angle in radians.
Since *tan* |pi|/4 = 1, we can set ``PI = 4 * ATN(1)``.

CHR$
----
Returns the character having the given ASCII code.
For example ``CHR$(65)`` is ``"A"``.
As a special case ``CHR$(0)`` is the empty string.
A run-time error occurs if ``CHR$`` is given a number
less than 0 or greater than 255, or a non-integer number.

COS
---
Returns the cosine of the angle given in radians.

EXP
---
The base *e* exponential function, so that ``EXP(1)`` is approximately 2.72.

INKEY$
------
On Windows,
returns the character for the key currently being pressed on the keyboard,
or the empty string if no key is being pressed.

Not implemented on Linux.

INT
---
Returns the integer part of the given number, rounding down.
So ``INT(3.1)`` = 3 and ``INT(-3.1)`` = -4.

LEFT$
-----
Returns the leftmost portion of a string.
For example ``LEFT$("Hello", 3)`` is “Hel”.
If the given number exceeds the length of the string, the whole string is returned.

LEN
---
Returns the length of the given string. So ``LEN("")`` = 0 and ``LEN("ABC")`` = 3.

LOG
---
Returns the base *e* logarithm (sometimes denoted *ln*)
of the given number, so that ``LOG(2.72)`` is approximately 1.
A run-time error occurs if the given number is not positive.

MID$
----
Returns a substring out of the middle of a string.
Takes a string, a starting position, and a length.
For example ``MID$("Hello",2,3)`` returns ``"ell"``.

If the length goes beyond the end of the string,
the whole string from the starting position is returned.

If the given starting position is not in the string,
for example ``MID$("ABC",4,1)``, a run-time error occurs.

RIGHT$
------
Returns the rightmost portion of a string.
For example ``RIGHT$("Hello", 2)`` is ``"lo"``.
If the given number exceeds the length of the string, the whole string is returned.

RND
---
Returns a pseudo-random number between 0 and 1:
specifically, computes *x* such that 0 <= *x* and *x* < 1.
The number is *pseudo*-random, not truly random, because it is computed.
See ``RANDOMIZE``.

SGN
---
Returns an indicator of the sign of the given number: 0 for 0, 1 for positive, -1 for negative.

SIN
---
Returns the sine of the angle given in radians.

SQR
---
Returns the square root of the given number, so that ``SQR(144)`` = 12.
Undefined if the given number is negative.

STR$
----
Returns the given number as a string, without spaces.
For example ``STR$(3.14)`` is "3.14", length 4.

TAN
---
Returns the tangent of the angle given in radians.

TIME$
-----
Returns the current time, in the local timezone, in the 24-hour clock,
for example ``"19:05:20"`` for 7.05pm and 20 seconds.

VAL
---
Returns the numeric value of the given string,
for example ``VAL("-3.14")`` is -3.14.

A run-time error occurs if the string does not contain a valid number,
for example ``VAL("20p")``.

This function only converts one given number;
it does not perform calculations, such as ``VAL("1+2")``.

========================
Operators and precedence
========================

Primary expressions
-------------------
Individual values in expressions, which can then be combined using operators, are:

1. Number constant, e.g. 123.
2. String constant, e.g. ``"Hello"``.
3. Simple variable, e.g. ``age``, ``name$``.
4. Array element, e.g. ``matrix(i, j)``.
5. Built-in function call, e.g. ``RND``, ``ASC(k$)``.
6. User-defined function call, e.g. ``FNA(0)``.
7. Parenthesised expression, e.g. ``(a+b*c)``.

Operators
---------
From highest to lowest precedence:

==============   =========================================
^                raise number to power
\-               negative number
\* /             multiplicative expression
\+ \-            additive expression, string concatenation
= < > <> <= >=   equality and relational expressions
NOT              bitwise not (complement)
AND              bitwise AND
OR               bitwise OR
==============   =========================================

Because the logical operators are bitwise, TRUE is best represented by -1 and FALSE by 0.


====================
Command line options
====================

The ``LegacyBasic`` (Windows) and ``legacy-basic`` (Linux) commands
take the following options.
Help on the options is also printed by running ``LegacyBasic –help-full``.
Most options have a single-letter form and a longer form.

--code -c
---------
Legacy Basic translates Basic source into an intermediate binary code,
which it then executes in a virtual machine.
This option lists the intermediate code for the input program,
instead of running the program.

--help -h
---------
Show program usage and list options.

--help-full -hh
---------------
Show program usage and explain all options.

--keywords-anywhere -k
----------------------
By default,
Legacy Basic requires keywords to be delimited with whitespace or punctuation,
for example ``LET A=4``.
Some Basics recognised keywords within delimited words,
for example ``LETA=4``.
This was called *crunching* words together.
This option recognises crunched keywords, anywhere outside a string literal.

--list -l
---------
List the source program.
Useful as a basic check that line numbers are distinct and in sequence,
and that Legacy Basic can load the program,
without running it or checking for syntax errors.

--list-names -n
---------------
List the names, as opposed to keywords, in the source program.

Flag names recognised as built-in functions, e.g. ``* SIN``,
and names recognised as printing operators, e.g. ``= TAB``.

The unflagged names are user-defined names.
If the interpreter considers a name user-defined,
it will not be interpreted as a built-in.

If a program is not running properly,
it might be because notation such as ``XXX(4)``
is being interpreted as an array element,
when it was intended to be a call of a built-in function.
This option will show that ``XXX`` is not recognised as a Legacy Basic built-in.
Legacy Basic will need extending in order to run that program.

--parse -p
----------
Parse the specified Basic program without running it,
to find syntax errors or unsupported constructs.

--quiet -q
----------
Suppress Legacy Basic version information.

--randomize -z
--------------
Randomize the random number generator,
seeding it from the current time,
so that ``RND`` produces a different sequence of numbers on each run.
Equivalent to using ``RANDOMIZE`` in the Basic program itself.

--report-memory -m
------------------
On exit, print the number of memory blocks allocated and released.
For debugging Legacy Basic's memory handling.

--run -r
--------
Run the specified Basic program. The default option.

--trace-basic -t
----------------
Trace Basic line numbers executed at runtime,
interspersed with normal output.
Equivalent to ``TRON`` and ``TRACE ON`` in some Basics.

--trace-for -f
--------------
Print information about ``FOR`` loops at runtime. For debugging the interpreter.

--trace-log -g
--------------
Print a detailed log of program execution to standard error output,
redirectable with ``2>`` .
Could be used to debug the Basic program,
but the amount of detail is intended for debugging the interpreter.

--unit-tests -unittest
----------------------
Only available if Legacy Basic was compiled with unit tests.
Run unit tests and print passes and failures.

--version -v
------------
Print Legacy Basic version information and exit.

Copyright |copy| 2023-24 Nigel Perks
