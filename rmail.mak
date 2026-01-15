.AUTODEPEND

.PATH.obj = UUCICO

#		*Translator Definitions*
CC = bcc +RMAIL.CFG
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
 rmail.obj \
 address.obj \
 arbmath.obj \
 arpadate.obj \
 chdir.obj \
 dater.obj \
 fakeport.obj \
 fakescr.obj \
 flock.obj \
 getopt.obj \
 getseq.obj \
 head.obj \
 hlib.obj \
 hostable.obj \
 import.obj \
 koi8.obj \
 lib.obj \
 mime.obj \
 pcmail.obj \
 sendone.obj \
 ssleep.obj \
 stat.obj \
 strpool.obj \
 swap.obj \
 timestmp.obj \
 tio.obj \
 tzset.obj \
 usertabl.obj \
 ..\exec\checkpcl.obj \
 ..\exec\execl.obj \
 ..\exec\spawncl.obj

#		*Explicit Rules*
uucico\rmail.exe: rmail.cfg $(EXE_dependencies)
  $(TLINK) /x/c/P-/L$(LIBPATH) @&&|
c0l.obj+
uucico\rmail.obj+
uucico\address.obj+
uucico\arbmath.obj+
uucico\arpadate.obj+
uucico\chdir.obj+
uucico\dater.obj+
uucico\fakeport.obj+
uucico\fakescr.obj+
uucico\flock.obj+
uucico\getopt.obj+
uucico\getseq.obj+
uucico\head.obj+
uucico\hlib.obj+
uucico\hostable.obj+
uucico\import.obj+
uucico\koi8.obj+
uucico\lib.obj+
uucico\mime.obj+
uucico\pcmail.obj+
uucico\sendone.obj+
uucico\ssleep.obj+
uucico\stat.obj+
uucico\strpool.obj+
uucico\swap.obj+
uucico\timestmp.obj+
uucico\tio.obj+
uucico\tzset.obj+
uucico\usertabl.obj+
..\exec\checkpcl.obj+
..\exec\execl.obj+
..\exec\spawncl.obj
uucico\rmail
		# no map file
cl.lib
|


#		*Individual File Dependencies*
rmail.obj: rmail.cfg rmail\rmail.c 
	$(CC) -c rmail\rmail.c

address.obj: rmail.cfg rmail\address.c 
	$(CC) -c rmail\address.c

arbmath.obj: rmail.cfg lib\arbmath.c 
	$(CC) -c lib\arbmath.c

arpadate.obj: rmail.cfg lib\arpadate.c 
	$(CC) -c lib\arpadate.c

chdir.obj: rmail.cfg lib\chdir.c 
	$(CC) -c lib\chdir.c

dater.obj: rmail.cfg lib\dater.c 
	$(CC) -c lib\dater.c

fakeport.obj: rmail.cfg lib\fakeport.c 
	$(CC) -c lib\fakeport.c

fakescr.obj: rmail.cfg lib\fakescr.c 
	$(CC) -c lib\fakescr.c

flock.obj: rmail.cfg lib\flock.c 
	$(CC) -c lib\flock.c

getopt.obj: rmail.cfg lib\getopt.c 
	$(CC) -c lib\getopt.c

getseq.obj: rmail.cfg lib\getseq.c 
	$(CC) -c lib\getseq.c

head.obj: rmail.cfg rmail\head.c 
	$(CC) -c rmail\head.c

hlib.obj: rmail.cfg lib\hlib.c 
	$(CC) -c lib\hlib.c

hostable.obj: rmail.cfg lib\hostable.c 
	$(CC) -c lib\hostable.c

import.obj: rmail.cfg lib\import.c 
	$(CC) -c lib\import.c

koi8.obj: rmail.cfg rmail\koi8.c 
	$(CC) -c rmail\koi8.c

lib.obj: rmail.cfg lib\lib.c 
	$(CC) -c lib\lib.c

mime.obj: rmail.cfg rmail\mime.c 
	$(CC) -c rmail\mime.c

ndir.obj: rmail.cfg lib\ndir.c 
	$(CC) -c lib\ndir.c

pcmail.obj: rmail.cfg rmail\pcmail.c 
	$(CC) -c rmail\pcmail.c

sendone.obj: rmail.cfg rmail\sendone.c 
	$(CC) -c rmail\sendone.c

ssleep.obj: rmail.cfg lib\ssleep.c 
	$(CC) -c lib\ssleep.c

stat.obj: rmail.cfg rmail\stat.c 
	$(CC) -c rmail\stat.c

strpool.obj: rmail.cfg lib\strpool.c 
	$(CC) -c lib\strpool.c

swap.obj: rmail.cfg lib\swap.c 
	$(CC) -c lib\swap.c

timestmp.obj: rmail.cfg lib\timestmp.c 
	$(CC) -c lib\timestmp.c

tzset.obj: rmail.cfg lib\tzset.c 
	$(CC) -c lib\tzset.c

tio.obj: rmail.cfg rmail\tio.c 
	$(CC) -c rmail\tio.c

usertabl.obj: rmail.cfg lib\usertabl.c 
	$(CC) -c lib\usertabl.c

#		*Compiler Configuration File*
rmail.cfg: rmail.mak
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
-wbbf
-wpin
-wamb
-wamp
-wasm
-wpro
-wcln
-wdef
-wsig
-wnod
-wstv
-wuse
-weas
-wpre
-nUUCICO
-I$(INCLUDEPATH)
-L$(LIBPATH)
-P-.C
-Ff
| rmail.cfg


