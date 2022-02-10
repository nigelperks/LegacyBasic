100 REM auto-dimension 10
110 PRINT A(3),B(2,1)
120 A(10)=99:B(10,10)=99
130 PRINT A(10),B(10,10)
200 REM auto-dimension higher
210 PRINT C(13),D(15,12)
220 PRINT C(13),D(15,12)
300 REM auto-dimension string
310 PRINT ">"A$(5)"<"
320 A$(10)="HELLO":PRINT A$(10)
400 REM explicit dimension
410 DIM name$(30),age(30)
420 K=30
430 name$(K)="Fred Smith":age(K)=141
440 PRINT name$(K)" is "str$(age(K))"!"
500 REM redimension
510 DIM age(4,5)
520 age(4,5)=99:age(0,0)=-33
530 PRINT age(0,0),age(4,5),age(3,2)
