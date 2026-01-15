.AUTODEPEND

.PATH.obj = UUCICO

#		*Translator Definitions*
CC = bcc +UUXQT.CFG
TASM = TASM
TLIB = tlib
TLINK = tlink
LIBPATH = \BC\LIB;
INCLUDEPATH = \BC\INCLUDE;..\EXEC;LIB;RMAIL


#		*Implicit Rules*
.c.obj:
  $(CC) -c {$< }

.cpp.obj:
  $(CC) -c {$< }

#		*List Macros*


EXE_dependencies =  \
 uuxqt.obj \
 arbmath.obj \
 arpadate.obj \
 chdir.obj \
 cp.obj \
 dater.obj \
 expath.obj \
 fakeport.obj \
 fakeslep.obj \
 flock.obj \
 getopt.obj \
 hlib.obj \
 hostable.obj \
 import.obj \
 koi8.obj \
 lib.obj \
 lock.obj \
 pushpop.obj \
 readnext.obj \
 screen.obj \
 stater.obj \
 strpool.obj \
 swap.obj \
 timestmp.obj \
 tzset.obj \
 usertabl.obj \
 ..\exec\execl.obj \
 ..\exec\checkpcl.obj \
 ..\exec\spawncl.obj

#		*Explicit Rules*
uucico\uuxqt.exe: uuxqt.cfg $(EXE_dependencies)
  $(TLINK) /x/c/P-/L$(LIBPATH) @&&|
c0l.obj+
uucico\uuxqt.obj+
uucico\arbmath.obj+
uucico\arpadate.obj+
uucico\chdir.obj+
uucico\cp.obj+
uucico\dater.obj+
uucico\expath.obj+
uucico\fakeport.obj+
uucico\fakeslep.obj+
uucico\flock.obj+
uucico\getopt.obj+
uucico\hlib.obj+
uucico\hostable.obj+
uucico\import.obj+
uucico\koi8.obj+
uucico\lib.obj+
uucico\lock.obj+
uucico\pushpop.obj+
uucico\readnext.obj+
uucico\screen.obj+
uucico\stater.obj+
uucico\strpool.obj+
uucico\swap.obj+
uucico\timestmp.obj+
uucico\tzset.obj+
uucico\usertabl.obj+
..\exec\execl.obj+
..\exec\checkpcl.obj+
..\exec\spawncl.obj
uucico\uuxqt
		# no map file
cl.lib
|


#		*Individual File Dependencies*
uuxqt.obj: uuxqt.cfg util\uuxqt.c 
	$(CC) -c util\uuxqt.c

arbmath.obj: uuxqt.cfg lib\arbmath.c 
	$(CC) -c lib\arbmath.c

arpadate.obj: uuxqt.cfg lib\arpadate.c 
	$(CC) -c lib\arpadate.c

chdir.obj: uuxqt.cfg lib\chdir.c 
	$(CC) -c lib\chdir.c

cp.obj: uuxqt.cfg lib\cp.c 
	$(CC) -c lib\cp.c

dater.obj: uuxqt.cfg lib\dater.c 
	$(CC) -c lib\dater.c

expath.obj: uuxqt.cfg lib\expath.c 
	$(CC) -c lib\expath.c

fakeport.obj: uuxqt.cfg lib\fakeport.c 
	$(CC) -c lib\fakeport.c

fakeslep.obj: uuxqt.cfg lib\fakeslep.c 
	$(CC) -c lib\fakeslep.c

flock.obj: uuxqt.cfg lib\flock.c 
	$(CC) -c lib\flock.c

getopt.obj: uuxqt.cfg lib\getopt.c 
	$(CC) -c lib\getopt.c

hlib.obj: uuxqt.cfg lib\hlib.c 
	$(CC) -c lib\hlib.c

hostable.obj: uuxqt.cfg lib\hostable.c 
	$(CC) -c lib\hostable.c

import.obj: uuxqt.cfg lib\import.c 
	$(CC) -c lib\import.c

koi8.obj: uuxqt.cfg rmail\koi8.c 
	$(CC) -c rmail\koi8.c

lib.obj: uuxqt.cfg lib\lib.c 
	$(CC) -c lib\lib.c

lock.obj: uuxqt.cfg lib\lock.c 
	$(CC) -c lib\lock.c

pushpop.obj: uuxqt.cfg lib\pushpop.c 
	$(CC) -c lib\pushpop.c

readnext.obj: uuxqt.cfg lib\readnext.c 
	$(CC) -c lib\readnext.c

screen.obj: uuxqt.cfg uucico\screen.c 
	$(CC) -c uucico\screen.c

stater.obj: uuxqt.cfg lib\stater.c 
	$(CC) -c lib\stater.c

strpool.obj: uuxqt.cfg lib\strpool.c 
	$(CC) -c lib\strpool.c

swap.obj: uuxqt.cfg lib\swap.c 
	$(CC) -c lib\swap.c

timestmp.obj: uuxqt.cfg lib\timestmp.c 
	$(CC) -c lib\timestmp.c

tzset.obj: uuxqt.cfg lib\tzset.c 
	$(CC) -c lib\tzset.c

usertabl.obj: uuxqt.cfg lib\usertabl.c 
	$(CC) -c lib\usertabl.c

#		*Compiler Configuration File*
uuxqt.cfg: uuxqt.mak
  copy &&|
-ml
-f-
-ff-
-G
-O
-Og
-Oe
-Ol
-Ob
-Z
-k-
-vi-
-wamb
-wamp
-wpro
-wdef
-wnod
-w-sus
-wstv
-wucp
-wuse
-weas
-wpre
-nUUCICO
-I$(INCLUDEPATH)
-L$(LIBPATH)
-P-.C
| uuxqt.cfg


