1000 A=2:B=5
1010 ON A GOSUB 1100,1200,1300
1020 ON B GOSUB 2100,2200,2300,2400
1030 PRINT "FALL THROUGH"
1040 GOTO 3000
1050 REM
1100 PRINT "ONE":RETURN
1200 PRINT "TWO":RETURN
1300 PRINT "THREE":RETURN
2100 PRINT "UN":RETURN
2200 PRINT "DEUX":RETURN
2300 PRINT "TROIS":RETURN
2400 PRINT "QUATRE":RETURN
3000 PRINT "END"
