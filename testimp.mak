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
 testimp.obj \
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
obj\testimp.exe: testimp.cfg $(EXE_dependencies)
  $(TLINK) /x/c/P-/L$(LIBPATH) @&&|
c0s.obj+
obj\testimp.obj+
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
obj\testimp
		# no map file
cs.lib
|


#		*Individual File Dependencies*
testimp.obj: testimp.cfg util\testimp.c 
	$(CC) -c util\testimp.c

arbmath.obj: testimp.cfg lib\arbmath.c 
	$(CC) -c lib\arbmath.c

chdir.obj: testimp.cfg lib\chdir.c 
	$(CC) -c lib\chdir.c

dater.obj: testimp.cfg lib\dater.c 
	$(CC) -c lib\dater.c

export.obj: testimp.cfg lib\export.c 
	$(CC) -c lib\export.c

fakescr.obj: testimp.cfg lib\fakescr.c 
	$(CC) -c lib\fakescr.c

fakeslep.obj: testimp.cfg lib\fakeslep.c 
	$(CC) -c lib\fakeslep.c

hlib.obj: testimp.cfg lib\hlib.c 
	$(CC) -c lib\hlib.c

hostable.obj: testimp.cfg lib\hostable.c 
	$(CC) -c lib\hostable.c

import.obj: testimp.cfg lib\import.c 
	$(CC) -c lib\import.c

lib.obj: testimp.cfg lib\lib.c 
	$(CC) -c lib\lib.c

strpool.obj: testimp.cfg lib\strpool.c 
	$(CC) -c lib\strpool.c

timestmp.obj: testimp.cfg lib\timestmp.c 
	$(CC) -c lib\timestmp.c

tzset.obj: testimp.cfg lib\tzset.c 
	$(CC) -c lib\tzset.c

#		*Compiler Configuration File*
testimp.cfg: testimp.mak
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
| testimp.cfg


