.AUTODEPEND

.PATH.obj = OBJ

#		*Translator Definitions*
CC = bcc +TESTIMP.CFG
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
 testexp.obj \
 arbmath.obj \
 chdir.obj \
 dater.obj \
 export.obj \
 fakescr.obj \
 fakeslep.obj \
 hlib.obj \
 hostable.obj \
 import.obj \
 lib.obj \
 strpool.obj \
 timestmp.obj \
 tzset.obj

#		*Explicit Rules*
obj\testexp.exe: testexp.cfg $(EXE_dependencies)
  $(TLINK) /v/l/x/c/P-/L$(LIBPATH) @&&|
c0s.obj+
obj\testexp.obj+
obj\arbmath.obj+
obj\chdir.obj+
obj\dater.obj+
obj\export.obj+
obj\fakescr.obj+
obj\fakeslep.obj+
obj\hlib.obj+
obj\hostable.obj+
obj\import.obj+
obj\lib.obj+
obj\strpool.obj+
obj\timestmp.obj+
obj\tzset.obj
obj\testexp
		# no map file
cs.lib
|


#		*Individual File Dependencies*
testexp.obj: testexp.cfg util\testexp.c
    $(CC) -c util\testexp.c

arbmath.obj: testexp.cfg lib\arbmath.c
	$(CC) -c lib\arbmath.c

chdir.obj: testexp.cfg lib\chdir.c
	$(CC) -c lib\chdir.c

dater.obj: testexp.cfg lib\dater.c
	$(CC) -c lib\dater.c

export.obj: testexp.cfg lib\export.c
	$(CC) -c lib\export.c

fakescr.obj: testexp.cfg lib\fakescr.c
	$(CC) -c lib\fakescr.c

fakeslep.obj: testexp.cfg lib\fakeslep.c
	$(CC) -c lib\fakeslep.c

hlib.obj: testexp.cfg lib\hlib.c
	$(CC) -c lib\hlib.c

hostable.obj: testexp.cfg lib\hostable.c
	$(CC) -c lib\hostable.c

import.obj: testexp.cfg lib\import.c
	$(CC) -c lib\import.c

lib.obj: testexp.cfg lib\lib.c
	$(CC) -c lib\lib.c

strpool.obj: testexp.cfg lib\strpool.c
	$(CC) -c lib\strpool.c

timestmp.obj: testexp.cfg lib\timestmp.c
	$(CC) -c lib\timestmp.c

tzset.obj: testexp.cfg lib\tzset.c
	$(CC) -c lib\tzset.c

#		*Compiler Configuration File*
testexp.cfg: testexp.mak
  copy &&|
-f-
-ff-
-C
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
-wasm
-wpro
-wdef
-wnod
-wstv
-wuse
-weas
-wpre
-nOBJ
-I$(INCLUDEPATH)
-L$(LIBPATH)
-P-.C
-v
-y
| testexp.cfg

