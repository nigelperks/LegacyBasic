10 REM random seed
20 RANDOMIZE
30 REM particular seed
40 RANDOMIZE 100
50 X = RND
60 RANDOMIZE 100
70 Y = RND
80 IF X = Y THEN PRINT "SEEDED"
