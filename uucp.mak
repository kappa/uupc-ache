.AUTODEPEND

.PATH.obj = OBJ

#		*Translator Definitions*
CC = bcc +UUCP.CFG
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
 uucp.obj \
 arbmath.obj \
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
 import.obj \
 lib.obj \
 normaliz.obj \
 pushpop.obj \
 strpool.obj \
 swap.obj \
 timestmp.obj \
 tzset.obj \
 usertabl.obj \
 ..\exec\checkpcs.obj \
 ..\exec\execs.obj \
 ..\exec\spawncs.obj \
 \bc\lib\wildargs.obj

#		*Explicit Rules*
obj\uucp.exe: uucp.cfg $(EXE_dependencies)
  $(TLINK) /x/c/P-/L$(LIBPATH) @&&|
c0s.obj+
obj\uucp.obj+
obj\arbmath.obj+
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
obj\import.obj+
obj\lib.obj+
obj\normaliz.obj+
obj\pushpop.obj+
obj\strpool.obj+
obj\swap.obj+
obj\timestmp.obj+
obj\tzset.obj+
obj\usertabl.obj+
..\exec\checkpcs.obj+
..\exec\execs.obj+
..\exec\spawncs.obj+
\bc\lib\wildargs.obj
obj\uucp
		# no map file
cs.lib
|


#		*Individual File Dependencies*
uucp.obj: uucp.cfg util\uucp.c 
	$(CC) -c util\uucp.c

arbmath.obj: uucp.cfg lib\arbmath.c 
	$(CC) -c lib\arbmath.c

chdir.obj: uucp.cfg lib\chdir.c 
	$(CC) -c lib\chdir.c

cp.obj: uucp.cfg lib\cp.c 
	$(CC) -c lib\cp.c

dater.obj: uucp.cfg lib\dater.c 
	$(CC) -c lib\dater.c

expath.obj: uucp.cfg lib\expath.c 
	$(CC) -c lib\expath.c

fakeport.obj: uucp.cfg lib\fakeport.c 
	$(CC) -c lib\fakeport.c

fakescr.obj: uucp.cfg lib\fakescr.c 
	$(CC) -c lib\fakescr.c

fakeslep.obj: uucp.cfg lib\fakeslep.c 
	$(CC) -c lib\fakeslep.c

flock.obj: uucp.cfg lib\flock.c 
	$(CC) -c lib\flock.c

getopt.obj: uucp.cfg lib\getopt.c 
	$(CC) -c lib\getopt.c

getseq.obj: uucp.cfg lib\getseq.c 
	$(CC) -c lib\getseq.c

hlib.obj: uucp.cfg lib\hlib.c 
	$(CC) -c lib\hlib.c

hostable.obj: uucp.cfg lib\hostable.c 
	$(CC) -c lib\hostable.c

import.obj: uucp.cfg lib\import.c 
	$(CC) -c lib\import.c

lib.obj: uucp.cfg lib\lib.c 
	$(CC) -c lib\lib.c

normaliz.obj: uucp.cfg lib\normaliz.c 
	$(CC) -c lib\normaliz.c

pushpop.obj: uucp.cfg lib\pushpop.c 
	$(CC) -c lib\pushpop.c

strpool.obj: uucp.cfg lib\strpool.c 
	$(CC) -c lib\strpool.c

swap.obj: uucp.cfg lib\swap.c 
	$(CC) -c lib\swap.c

timestmp.obj: uucp.cfg lib\timestmp.c 
	$(CC) -c lib\timestmp.c

tzset.obj: uucp.cfg lib\tzset.c 
	$(CC) -c lib\tzset.c

usertabl.obj: uucp.cfg lib\usertabl.c 
	$(CC) -c lib\usertabl.c

#		*Compiler Configuration File*
uucp.cfg: uucp.mak
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
-w-sus
-weas
-wpre
-nOBJ
-I$(INCLUDEPATH)
-L$(LIBPATH)
-P-.C
| uucp.cfg


