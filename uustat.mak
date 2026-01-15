.AUTODEPEND

.PATH.obj = OBJ

#		*Translator Definitions*
CC = bcc +UUSTAT.CFG
TASM = TASM
TLIB = tlib
TLINK = tlink
LIBPATH = \BC\LIB
INCLUDEPATH = \BC\INCLUDE;LIB


#		*Implicit Rules*
.c.obj:
  $(CC) -c {$< }

.cpp.obj:
  $(CC) -c {$< }

#		*List Macros*


EXE_dependencies =  \
 uustat.obj \
 arbmath.obj \
 chdir.obj \
 dater.obj \
 export.obj \
 fakeport.obj \
 fakescr.obj \
 fakeslep.obj \
 flock.obj \
 getopt.obj \
 getseq.obj \
 hlib.obj \
 hostable.obj \
 hostatus.obj \
 import.obj \
 lib.obj \
 pushpop.obj \
 readnext.obj \
 stater.obj \
 strpool.obj \
 timestmp.obj \
 tzset.obj

#		*Explicit Rules*
obj\uustat.exe: uustat.cfg $(EXE_dependencies)
  $(TLINK) /x/c/P-/L$(LIBPATH) @&&|
c0s.obj+
obj\uustat.obj+
obj\arbmath.obj+
obj\chdir.obj+
obj\dater.obj+
obj\export.obj+
obj\fakeport.obj+
obj\fakescr.obj+
obj\fakeslep.obj+
obj\flock.obj+
obj\getopt.obj+
obj\getseq.obj+
obj\hlib.obj+
obj\hostable.obj+
obj\hostatus.obj+
obj\import.obj+
obj\lib.obj+
obj\pushpop.obj+
obj\readnext.obj+
obj\stater.obj+
obj\strpool.obj+
obj\timestmp.obj+
obj\tzset.obj
obj\uustat
		# no map file
cs.lib
|


#		*Individual File Dependencies*
uustat.obj: uustat.cfg util\uustat.c 
	$(CC) -c util\uustat.c

arbmath.obj: uustat.cfg lib\arbmath.c 
	$(CC) -c lib\arbmath.c

chdir.obj: uustat.cfg lib\chdir.c 
	$(CC) -c lib\chdir.c

dater.obj: uustat.cfg lib\dater.c 
	$(CC) -c lib\dater.c

export.obj: uustat.cfg lib\export.c 
	$(CC) -c lib\export.c

fakeport.obj: uustat.cfg lib\fakeport.c 
	$(CC) -c lib\fakeport.c

fakescr.obj: uustat.cfg lib\fakescr.c 
	$(CC) -c lib\fakescr.c

fakeslep.obj: uustat.cfg lib\fakeslep.c 
	$(CC) -c lib\fakeslep.c

flock.obj: uustat.cfg lib\flock.c 
	$(CC) -c lib\flock.c

getopt.obj: uustat.cfg lib\getopt.c 
	$(CC) -c lib\getopt.c

getseq.obj: uustat.cfg lib\getseq.c 
	$(CC) -c lib\getseq.c

hlib.obj: uustat.cfg lib\hlib.c 
	$(CC) -c lib\hlib.c

hostable.obj: uustat.cfg lib\hostable.c 
	$(CC) -c lib\hostable.c

hostatus.obj: uustat.cfg lib\hostatus.c 
	$(CC) -c lib\hostatus.c

import.obj: uustat.cfg lib\import.c 
	$(CC) -c lib\import.c

lib.obj: uustat.cfg lib\lib.c 
	$(CC) -c lib\lib.c

pushpop.obj: uustat.cfg lib\pushpop.c 
	$(CC) -c lib\pushpop.c

readnext.obj: uustat.cfg lib\readnext.c 
	$(CC) -c lib\readnext.c

stater.obj: uustat.cfg lib\stater.c 
	$(CC) -c lib\stater.c

strpool.obj: uustat.cfg lib\strpool.c 
	$(CC) -c lib\strpool.c

timestmp.obj: uustat.cfg lib\timestmp.c 
	$(CC) -c lib\timestmp.c

tzset.obj: uustat.cfg lib\tzset.c 
	$(CC) -c lib\tzset.c

#		*Compiler Configuration File*
uustat.cfg: uustat.mak
  copy &&|
-f-
-ff-
-j10
-g10
-G
-O
-Og
-Oe
-Ol
-Ob
-Z
-k-
-vi-
-wpin
-wamb
-wamp
-wpro
-wdef
-wsig
-wnod
-wstv
-wucp
-wuse
-weas
-wpre
-nOBJ
-I$(INCLUDEPATH)
-L$(LIBPATH)
-P-.C
| uustat.cfg


