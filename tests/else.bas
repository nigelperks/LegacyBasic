10 LET A=10
20 LET B=20
30 LET C=30
100 IF A<B THEN PRINT "A<B" ELSE PRINT "---"
110 IF A>B THEN PRINT "A>B" ELSE PRINT "---"
120 IF A<C THEN PRINT "A<C":IF B<C THEN PRINT "B<C" ELSE PRINT "---"
130 IF A>C THEN PRINT "A>C" ELSE IF B>C THEN PRINT "B>C" ELSE PRINT "C MAX"
140 IF A>10 THEN ELSE PRINT "NOT A>10"
150 IF B>C THEN PRINT "P";:PRINT "Q"; ELSE PRINT "R";:PRINT "S";:IF B>A THEN PRINT "T";:PRINT "U"; ELSE PRINT "X";
160 PRINT
170 IF A<C THEN PRINT "A<C" ELSE PRINT "---":IF A<B THEN PRINT "A<B"