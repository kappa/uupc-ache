.AUTODEPEND

.PATH.obj = OBJ

#		*Translator Definitions*
CC = bcc +BATCH.CFG
TASM = TASM
TLIB = tlib
TLINK = tlink
LIBPATH = \BC\LIB
INCLUDEPATH = \BC\INCLUDE;..\EXEC;LIB


#		*Implicit Rules*
.c.obj:
  $(CC) -c {$< }

.cpp.obj:
  $(CC) -c {$< }

#		*List Macros*


EXE_dependencies =  \
 batch.obj \
 arbmath.obj \
 arpadate.obj \
 chdir.obj \
 cp.obj \
 dater.obj \
 expath.obj \
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
 lock.obj \
 pushpop.obj \
 stater.obj \
 strpool.obj \
 swap.obj \
 timestmp.obj \
 tzset.obj \
 usertabl.obj \
 ..\exec\checkpcs.obj \
 ..\exec\execs.obj \
 ..\exec\spawncs.obj

#		*Explicit Rules*
obj\batch.exe: batch.cfg $(EXE_dependencies)
  $(TLINK) /x/c/P-/L$(LIBPATH) @&&|
c0s.obj+
obj\batch.obj+
obj\arbmath.obj+
obj\arpadate.obj+
obj\chdir.obj+
obj\cp.obj+
obj\dater.obj+
obj\expath.obj+
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
obj\lock.obj+
obj\pushpop.obj+
obj\stater.obj+
obj\strpool.obj+
obj\swap.obj+
obj\timestmp.obj+
obj\tzset.obj+
obj\usertabl.obj+
..\exec\checkpcs.obj+
..\exec\execs.obj+
..\exec\spawncs.obj
obj\batch
		# no map file
cs.lib
|


#		*Individual File Dependencies*
batch.obj: batch.cfg util\batch.c 
	$(CC) -c util\batch.c

arbmath.obj: batch.cfg lib\arbmath.c 
	$(CC) -c lib\arbmath.c

arpadate.obj: batch.cfg lib\arpadate.c 
	$(CC) -c lib\arpadate.c

chdir.obj: batch.cfg lib\chdir.c 
	$(CC) -c lib\chdir.c

cp.obj: batch.cfg lib\cp.c 
	$(CC) -c lib\cp.c

dater.obj: batch.cfg lib\dater.c 
	$(CC) -c lib\dater.c

expath.obj: batch.cfg lib\expath.c 
	$(CC) -c lib\expath.c

fakeport.obj: batch.cfg lib\fakeport.c 
	$(CC) -c lib\fakeport.c

fakescr.obj: batch.cfg lib\fakescr.c 
	$(CC) -c lib\fakescr.c

fakeslep.obj: batch.cfg lib\fakeslep.c 
	$(CC) -c lib\fakeslep.c

flock.obj: batch.cfg lib\flock.c 
	$(CC) -c lib\flock.c

getopt.obj: batch.cfg lib\getopt.c 
	$(CC) -c lib\getopt.c

getseq.obj: batch.cfg lib\getseq.c 
	$(CC) -c lib\getseq.c

hlib.obj: batch.cfg lib\hlib.c 
	$(CC) -c lib\hlib.c

hostable.obj: batch.cfg lib\hostable.c 
	$(CC) -c lib\hostable.c

hostatus.obj: batch.cfg lib\hostatus.c 
	$(CC) -c lib\hostatus.c

import.obj: batch.cfg lib\import.c 
	$(CC) -c lib\import.c

lib.obj: batch.cfg lib\lib.c 
	$(CC) -c lib\lib.c

lock.obj: batch.cfg lib\lock.c 
	$(CC) -c lib\lock.c

ndir.obj: batch.cfg lib\ndir.c 
	$(CC) -c lib\ndir.c

pushpop.obj: batch.cfg lib\pushpop.c 
	$(CC) -c lib\pushpop.c

stater.obj: batch.cfg lib\stater.c 
	$(CC) -c lib\stater.c

strpool.obj: batch.cfg lib\strpool.c 
	$(CC) -c lib\strpool.c

swap.obj: batch.cfg lib\swap.c 
	$(CC) -c lib\swap.c

timestmp.obj: batch.cfg lib\timestmp.c 
	$(CC) -c lib\timestmp.c

tzset.obj: batch.cfg lib\tzset.c 
	$(CC) -c lib\tzset.c

usertabl.obj: batch.cfg lib\usertabl.c 
	$(CC) -c lib\usertabl.c

#		*Compiler Configuration File*
batch.cfg: batch.mak
  copy &&|
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
-weas
-wpre
-nOBJ
-I$(INCLUDEPATH)
-L$(LIBPATH)
-P-.C
| batch.cfg


