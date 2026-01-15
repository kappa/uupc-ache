.AUTODEPEND

.PATH.obj = OBJ

#		*Translator Definitions*
CC = bcc +UUPOLL.CFG
TASM = TASM
TLIB = tlib
TLINK = tlink
LIBPATH = \BC\LIB;
INCLUDEPATH = \BC\INCLUDE;..\EXEC;LIB


#		*Implicit Rules*
.c.obj:
  $(CC) -c {$< }

.cpp.obj:
  $(CC) -c {$< }

#		*List Macros*


EXE_dependencies =  \
 uupoll.obj \
 arpadate.obj \
 chdir.obj \
 dater.obj \
 fakeport.obj \
 fakescr.obj \
 flock.obj \
 getopt.obj \
 hlib.obj \
 hostable.obj \
 lib.obj \
 ssleep.obj \
 swap.obj \
 strpool.obj \
 timestmp.obj \
 tzset.obj \
 ..\exec\checkpcs.obj \
 ..\exec\execs.obj \
 ..\exec\spawncs.obj

#		*Explicit Rules*
obj\uupoll.exe: uupoll.cfg $(EXE_dependencies)
  $(TLINK) /x/c/P-/L$(LIBPATH) @&&|
c0s.obj+
obj\uupoll.obj+
obj\arpadate.obj+
obj\chdir.obj+
obj\dater.obj+
obj\fakeport.obj+
obj\fakescr.obj+
obj\flock.obj+
obj\getopt.obj+
obj\hlib.obj+
obj\hostable.obj+
obj\lib.obj+
obj\ssleep.obj+
obj\swap.obj+
obj\strpool.obj+
obj\timestmp.obj+
obj\tzset.obj+
..\exec\checkpcs.obj+
..\exec\execs.obj+
..\exec\spawncs.obj
obj\uupoll
		# no map file
cs.lib
|


#		*Individual File Dependencies*
uupoll.obj: uupoll.cfg util\uupoll.c 
	$(CC) -c util\uupoll.c

arpadate.obj: uupoll.cfg lib\arpadate.c 
	$(CC) -c lib\arpadate.c

chdir.obj: uupoll.cfg lib\chdir.c 
	$(CC) -c lib\chdir.c

dater.obj: uupoll.cfg lib\dater.c 
	$(CC) -c lib\dater.c

fakeport.obj: uupoll.cfg lib\fakeport.c 
	$(CC) -c lib\fakeport.c

fakescr.obj: uupoll.cfg lib\fakescr.c 
	$(CC) -c lib\fakescr.c

flock.obj: uupoll.cfg lib\flock.c 
	$(CC) -c lib\flock.c

getopt.obj: uupoll.cfg lib\getopt.c 
	$(CC) -c lib\getopt.c

hlib.obj: uupoll.cfg lib\hlib.c 
	$(CC) -c lib\hlib.c

hostable.obj: uupoll.cfg lib\hostable.c 
	$(CC) -c lib\hostable.c

lib.obj: uupoll.cfg lib\lib.c 
	$(CC) -c lib\lib.c

ssleep.obj: uupoll.cfg lib\ssleep.c 
	$(CC) -c lib\ssleep.c

swap.obj: uupoll.cfg lib\swap.c 
	$(CC) -c lib\swap.c

strpool.obj: uupoll.cfg lib\strpool.c 
	$(CC) -c lib\strpool.c

timestmp.obj: uupoll.cfg lib\timestmp.c 
	$(CC) -c lib\timestmp.c

tzset.obj: uupoll.cfg lib\tzset.c 
	$(CC) -c lib\tzset.c

#		*Compiler Configuration File*
uupoll.cfg: uupoll.mak
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
| uupoll.cfg


