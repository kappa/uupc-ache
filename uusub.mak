.AUTODEPEND

.PATH.obj = OBJ

#		*Translator Definitions*
CC = bcc +UUSUB.CFG
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
 uusub.obj \
 arbmath.obj \
 chdir.obj \
 dater.obj \
 fakeport.obj \
 fakescr.obj \
 fakeslep.obj \
 getopt.obj \
 hlib.obj \
 hostable.obj \
 hostrset.obj \
 hostatus.obj \
 import.obj \
 lib.obj \
 lock.obj \
 stater.obj \
 strpool.obj \
 timestmp.obj \
 tzset.obj

#		*Explicit Rules*
obj\uusub.exe: uusub.cfg $(EXE_dependencies)
  $(TLINK) /x/c/P-/L$(LIBPATH) @&&|
c0s.obj+
obj\uusub.obj+
obj\arbmath.obj+
obj\chdir.obj+
obj\dater.obj+
obj\fakeport.obj+
obj\fakescr.obj+
obj\fakeslep.obj+
obj\getopt.obj+
obj\hlib.obj+
obj\hostable.obj+
obj\hostrset.obj+
obj\hostatus.obj+
obj\import.obj+
obj\lib.obj+
obj\lock.obj+
obj\stater.obj+
obj\strpool.obj+
obj\timestmp.obj+
obj\tzset.obj
obj\uusub
		# no map file
cs.lib
|


#		*Individual File Dependencies*
uusub.obj: uusub.cfg util\uusub.c 
	$(CC) -c util\uusub.c

arbmath.obj: uusub.cfg lib\arbmath.c 
	$(CC) -c lib\arbmath.c

chdir.obj: uusub.cfg lib\chdir.c 
	$(CC) -c lib\chdir.c

dater.obj: uusub.cfg lib\dater.c 
	$(CC) -c lib\dater.c

fakeport.obj: uusub.cfg lib\fakeport.c 
	$(CC) -c lib\fakeport.c

fakescr.obj: uusub.cfg lib\fakescr.c 
	$(CC) -c lib\fakescr.c

fakeslep.obj: uusub.cfg lib\fakeslep.c 
	$(CC) -c lib\fakeslep.c

getopt.obj: uusub.cfg lib\getopt.c 
	$(CC) -c lib\getopt.c

hlib.obj: uusub.cfg lib\hlib.c 
	$(CC) -c lib\hlib.c

hostable.obj: uusub.cfg lib\hostable.c 
	$(CC) -c lib\hostable.c

hostrset.obj: uusub.cfg lib\hostrset.c 
	$(CC) -c lib\hostrset.c

hostatus.obj: uusub.cfg lib\hostatus.c 
	$(CC) -c lib\hostatus.c

import.obj: uusub.cfg lib\import.c 
	$(CC) -c lib\import.c

lib.obj: uusub.cfg lib\lib.c 
	$(CC) -c lib\lib.c

lock.obj: uusub.cfg lib\lock.c 
	$(CC) -c lib\lock.c

stater.obj: uusub.cfg lib\stater.c 
	$(CC) -c lib\stater.c

strpool.obj: uusub.cfg lib\strpool.c 
	$(CC) -c lib\strpool.c

timestmp.obj: uusub.cfg lib\timestmp.c 
	$(CC) -c lib\timestmp.c

tzset.obj: uusub.cfg lib\tzset.c 
	$(CC) -c lib\tzset.c

#		*Compiler Configuration File*
uusub.cfg: uusub.mak
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
| uusub.cfg


