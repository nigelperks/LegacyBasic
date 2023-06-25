10 REM Incredibly simple text adventure to demonstrate Basic.
12 REM Verb suggestions: look,redescribe,inventory,examine,help,quit.
20 GOSUB 2000
25 FALSE=0:TRUE=-1
28 HIDDEN=-1:HELD=-2
30 room=0
40 locked=TRUE
50 opened=FALSE
60 keyloc=HIDDEN
100 ON room+1 GOTO 1000,1100
110 IF room=0 AND locked THEN PRINT "The door is locked."
112 IF room=0 AND NOT locked THEN PRINT "The door is unlocked."
120 IF room=0 AND opened THEN PRINT "The drawer is open."
122 IF room=0 AND NOT opened THEN PRINT "The drawer is closed."
130 IF keyloc = room THEN PRINT "A key is here."
200 REM get command
205 PRINT
210 LINE INPUT "What do you want to do"; line$
215 PRINT
220 L=len(line$)
230 i=1
240 IF i <= L THEN IF MID$(line$,i,1)=" " THEN i=i+1:GOTO 240
250 IF i > L THEN 210
260 verb$=""
270 verb$=verb$+MID$(line$,i,1)
280 i=i+1
290 IF i <= L THEN IF MID$(line$,i,1)<>" " THEN 270
300 IF i <= L THEN IF MID$(line$,i,1)=" " THEN i=i+1:GOTO 300
310 noun$=""
320 IF i > L THEN 400
330 noun$=noun$+MID$(line$,i,1)
340 i=i+1
350 IF i <= L THEN IF MID$(line$,i,1)<>" " THEN 330
360 IF i <= L THEN IF MID$(line$,i,1)=" " THEN i=i+1:GOTO 360
370 IF i > L THEN 400
380 PRINT "Please use no more than one verb and one noun."
390 GOTO 200
400 REM PRINT "Verb >";verb$;"< noun >";noun$;"<"
410 REM is it a direction?
420 IF (verb$="go" or verb$="move") AND noun$<>"" THEN dir$=noun$:GOTO 500
430 IF noun$="" THEN dir$=verb$:GOTO 500
440 GOTO 700
500 FOR i = 0 TO MAXDIR
510 IF dir$=LEFT$(dirs$(i),LEN(dir$)) THEN dir=i:GOTO 600
520 NEXT i
530 GOTO 700
600 REM it is a direction
610 IF exits(room,dir)=-1 THEN PRINT "You can't go that way.":GOTO 200
620 room=exits(room,dir)
630 GOTO 100
700 REM find verb and noun when not a direction
710 verb=-1:noun=-1
720 FOR i = 0 TO MAXVERB
730 IF verbs$(i)=verb$ THEN verb=i:GOTO 770
740 NEXT i
750 PRINT "I don't understand "; verb$;" as a verb."
760 GOTO 200
770 IF noun$="" THEN 830
775 IF LEN(noun$)<3 THEN 810
780 FOR i = 0 TO MAXNOUN
790 IF LEFT$(nouns$(i),LEN(noun$))=noun$ THEN noun=i:GOTO 830
800 NEXT i
810 PRINT "I don't understand "; noun$;" as a noun."
820 GOTO 200
830 REM found verb and noun
840 REM PRINT "verb";verb;verbs$(verb)
850 REM IF noun>=0 THEN PRINT "noun";noun;nouns$(noun)
860 ON verb+1 GOTO 3000,3100,3200,3300,3400,3500,3600,3700
1000 REM room 0
1010 PRINT "You are in a room, totally bare except for a desk with a single drawer."
1020 PRINT "There is a door in the east wall."
1030 GOTO 110
1100 REM room 1
1110 PRINT "You are outside the room. You have escaped! You have won! Congratulations!"
1120 PRINT
1130 END
2000 REM initialise vocabulary
2010 MAXDIR=3
2020 DIM dirs$(3)
2030 FOR i = 0 TO MAXDIR:READ dirs$(i):NEXT i
2040 DATA "north","south","east","west"
2100 MAXVERB=7
2110 DIM verbs$(7)
2120 FOR i = 0 TO MAXVERB:READ verbs$(i):NEXT i
2130 DATA "go","move","get","drop","open","close","unlock","lock"
2200 MAXNOUN=4
2210 DIM nouns$(4)
2220 FOR i = 0 TO MAXNOUN:READ nouns$(i):NEXT i
2230 DATA "room","desk","drawer","door","key"
2300 REM initialise exits
2310 MAXROOM=1:MAXEXIT=3
2320 DIM exits(1,3)
2330 FOR i = 0 TO MAXROOM
2340 FOR j = 0 TO MAXEXIT:READ exits(i,j):NEXT j
2350 NEXT i
2360 DATA -1,-1,-1,-1
2361 DATA -1,-1,-1,-1
2400 RETURN
3000 REM go
3005 IF noun=-1 THEN PRINT "Go where?":GOTO 200
3010 PRINT "That doesn't make sense."
3020 GOTO 200
3100 REM move
3105 IF noun=-1 THEN PRINT "Move where?":GOTO 200
3110 PRINT "You can't move that."
3120 GOTO 200
3200 REM get
3205 IF noun=-1 THEN PRINT "Get what?":GOTO 200
3210 IF noun<>4 THEN PRINT "You can't get that.":GOTO 200
3220 IF keyloc<>room THEN PRINT "It isn't here.":GOTO 200
3230 keyloc=HELD
3235 PRINT "You now have the ";nouns$(noun);"."
3240 GOTO 200
3300 REM drop
3305 IF noun=-1 THEN PRINT "Drop what?":GOTO 200
3310 IF noun<>4 OR keyloc<>-2 THEN PRINT "You don't have that.":GOTO 200
3320 keyloc=room
3330 PRINT "You have dropped the ";nouns$(noun);"."
3340 GOTO 200
3400 REM open
3405 IF noun=-1 THEN PRINT "Open what?":GOTO 200
3410 IF noun<>2 OR room<>0 THEN PRINT "You can't open that.":GOTO 200
3420 IF opened THEN PRINT "It's already open.":GOTO 200
3430 opened=TRUE
3435 IF keyloc=-1 THEN keyloc=room
3440 GOTO 100
3500 REM close
3510 IF noun=-1 THEN PRINT "Close what?":GOTO 200
3520 IF room<>0 OR noun<>2 THEN PRINT "You can't close that.":GOTO 200
3530 IF NOT opened THEN PRINT "The drawer is already closed.":GOTO 200
3540 opened=FALSE
3550 IF keyloc=room THEN keyloc=HIDDEN
3560 PRINT "The drawer is now closed."
3570 GOTO 200
3600 REM unlock
3605 IF noun=-1 THEN PRINT "Unlock what?":GOTO 200
3610 IF room<>0 OR noun<>3 THEN PRINT "You can't unlock that.":GOTO 200
3620 IF keyloc <> HELD THEN PRINT "You don't have the key.":GOTO 200
3625 IF NOT locked THEN PRINT "The door is already unlocked.":GOTO 200
3630 locked=FALSE
3635 exits(0,2)=1
3640 GOTO 100
3700 REM lock
3705 IF noun=-1 THEN PRINT "Lock what?":GOTO 200
3710 IF room<>0 OR noun<>3 THEN PRINT "You can't lock that.":GOTO 200
3720 IF keyloc <> HELD THEN PRINT "You don't have the key.":GOTO 200
3725 IF locked THEN PRINT "The door is already locked.":GOTO 200
3730 locked=TRUE
3735 exits(0,2)=-1
3740 GOTO 100
