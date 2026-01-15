.AUTODEPEND

.PATH.obj = OBJ

#		*Translator Definitions*
CC = bcc +UUX.CFG
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
 uux.obj \
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
obj\uux.exe: uux.cfg $(EXE_dependencies)
  $(TLINK) /x/c/P-/L$(LIBPATH) @&&|
c0s.obj+
obj\uux.obj+
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
obj\uux
		# no map file
cs.lib
|


#		*Individual File Dependencies*
uux.obj: uux.cfg util\uux.c 
	$(CC) -c util\uux.c

arbmath.obj: uux.cfg lib\arbmath.c 
	$(CC) -c lib\arbmath.c

arpadate.obj: uux.cfg lib\arpadate.c 
	$(CC) -c lib\arpadate.c

chdir.obj: uux.cfg lib\chdir.c 
	$(CC) -c lib\chdir.c

cp.obj: uux.cfg lib\cp.c 
	$(CC) -c lib\cp.c

dater.obj: uux.cfg lib\dater.c 
	$(CC) -c lib\dater.c

expath.obj: uux.cfg lib\expath.c 
	$(CC) -c lib\expath.c

fakeport.obj: uux.cfg lib\fakeport.c 
	$(CC) -c lib\fakeport.c

fakescr.obj: uux.cfg lib\fakescr.c 
	$(CC) -c lib\fakescr.c

fakeslep.obj: uux.cfg lib\fakeslep.c 
	$(CC) -c lib\fakeslep.c

flock.obj: uux.cfg lib\flock.c 
	$(CC) -c lib\flock.c

getopt.obj: uux.cfg lib\getopt.c 
	$(CC) -c lib\getopt.c

getseq.obj: uux.cfg lib\getseq.c 
	$(CC) -c lib\getseq.c

hlib.obj: uux.cfg lib\hlib.c 
	$(CC) -c lib\hlib.c

hostable.obj: uux.cfg lib\hostable.c 
	$(CC) -c lib\hostable.c

hostatus.obj: uux.cfg lib\hostatus.c 
	$(CC) -c lib\hostatus.c

import.obj: uux.cfg lib\import.c 
	$(CC) -c lib\import.c

lib.obj: uux.cfg lib\lib.c 
	$(CC) -c lib\lib.c

pushpop.obj: uux.cfg lib\pushpop.c 
	$(CC) -c lib\pushpop.c

stater.obj: uux.cfg lib\stater.c 
	$(CC) -c lib\stater.c

strpool.obj: uux.cfg lib\strpool.c 
	$(CC) -c lib\strpool.c

swap.obj: uux.cfg lib\swap.c 
	$(CC) -c lib\swap.c

timestmp.obj: uux.cfg lib\timestmp.c 
	$(CC) -c lib\timestmp.c

tzset.obj: uux.cfg lib\tzset.c 
	$(CC) -c lib\tzset.c

usertabl.obj: uux.cfg lib\usertabl.c 
	$(CC) -c lib\usertabl.c

#		*Compiler Configuration File*
uux.cfg: uux.mak
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
-wbbf
-wpin
-wamb
-wamp
-wasm
-wpro
-wcln
-wdef
-wnod
-w-sus
-wstv
-wucp
-wuse
-weas
-wpre
-nOBJ
-I$(INCLUDEPATH)
-L$(LIBPATH)
-P-.C
| uux.cfg


