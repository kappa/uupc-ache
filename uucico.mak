.AUTODEPEND

.PATH.obj = UUCICO

#		*Translator Definitions*
CC = bcc +UUCICO.CFG
TASM = TASM
TLIB = tlib
#TLINK = tlink /l /v
TLINK = tlink
LIBPATH = \BC\LIB;..\WATTCP\LIB;
INCLUDEPATH = \BC\INCLUDE;LIB;UUCICO;..\WATTCP\INCLUDE;..\EXEC


#		*Implicit Rules*
.c.obj:
  $(CC) -c {$< }

.cpp.obj:
  $(CC) -c {$< }

#		*List Macros*


EXE_dependencies =  \
 uucico.obj \
 arbmath.obj \
 arpadate.obj \
 chdir.obj \
 checktim.obj \
 dater.obj \
 dcp.obj \
 dcpgpkt.obj \
 dcplib.obj \
 dcpstats.obj \
 dcpsys.obj \
 dcptpkt.obj \
 dcpxfer.obj \
 expath.obj \
 flock.obj \
 fossil.obj \
 getopt.obj \
 hlib.obj \
 hostable.obj \
 hostatus.obj \
 import.obj \
 lib.obj \
 lock.obj \
 modem.obj \
 mx5.obj \
 pushpop.obj \
 screen.obj \
 script.obj \
 ssleep.obj \
 stater.obj \
 strpool.obj \
 swap.obj \
 timestmp.obj \
 tzset.obj \
 ulib.obj \
 usertabl.obj \
 commfifo.obj \
 ..\wattcp\lib\wattcplg.lib \
 ..\exec\execl.obj \
 ..\exec\checkpcl.obj \
 ..\exec\spawncl.obj

#		*Explicit Rules*
uucico\uucico.exe: uucico.cfg $(EXE_dependencies)
  $(TLINK) /x/c/P-/L$(LIBPATH) @&&|
c0l.obj+
uucico\uucico.obj+
uucico\arbmath.obj+
uucico\arpadate.obj+
uucico\chdir.obj+
uucico\checktim.obj+
uucico\dater.obj+
uucico\dcp.obj+
uucico\dcpgpkt.obj+
uucico\dcplib.obj+
uucico\dcpstats.obj+
uucico\dcpsys.obj+
uucico\dcptpkt.obj+
uucico\dcpxfer.obj+
uucico\expath.obj+
uucico\flock.obj+
uucico\fossil.obj+
uucico\getopt.obj+
uucico\hlib.obj+
uucico\hostable.obj+
uucico\hostatus.obj+
uucico\import.obj+
uucico\lib.obj+
uucico\lock.obj+
uucico\modem.obj+
uucico\mx5.obj+
uucico\pushpop.obj+
uucico\screen.obj+
uucico\script.obj+
uucico\ssleep.obj+
uucico\stater.obj+
uucico\strpool.obj+
uucico\swap.obj+
uucico\timestmp.obj+
uucico\tzset.obj+
uucico\ulib.obj+
uucico\usertabl.obj+
uucico\commfifo.obj+
..\exec\execl.obj+
..\exec\checkpcl.obj+
..\exec\spawncl.obj
uucico\uucico
		# no map file
..\wattcp\lib\wattcplg.lib+
cl.lib
|


#		*Individual File Dependencies*
uucico.obj: uucico.cfg uucico\uucico.c 
	$(CC) -c uucico\uucico.c

arbmath.obj: uucico.cfg lib\arbmath.c 
	$(CC) -c lib\arbmath.c

arpadate.obj: uucico.cfg lib\arpadate.c 
	$(CC) -c lib\arpadate.c

chdir.obj: uucico.cfg lib\chdir.c 
	$(CC) -c lib\chdir.c

checktim.obj: uucico.cfg uucico\checktim.c 
	$(CC) -c uucico\checktim.c

dater.obj: uucico.cfg lib\dater.c 
	$(CC) -c lib\dater.c

dcp.obj: uucico.cfg uucico\dcp.c 
	$(CC) -c uucico\dcp.c

dcpgpkt.obj: uucico.cfg uucico\dcpgpkt.c 
	$(CC) -c uucico\dcpgpkt.c

dcplib.obj: uucico.cfg uucico\dcplib.c 
	$(CC) -c uucico\dcplib.c

dcpstats.obj: uucico.cfg uucico\dcpstats.c 
	$(CC) -c uucico\dcpstats.c

dcpsys.obj: uucico.cfg uucico\dcpsys.c 
	$(CC) -c uucico\dcpsys.c

dcptpkt.obj: uucico.cfg uucico\dcptpkt.c 
	$(CC) -c uucico\dcptpkt.c

dcpxfer.obj: uucico.cfg uucico\dcpxfer.c 
	$(CC) -c uucico\dcpxfer.c

expath.obj: uucico.cfg lib\expath.c 
	$(CC) -c lib\expath.c

flock.obj: uucico.cfg lib\flock.c 
	$(CC) -c lib\flock.c

fossil.obj: uucico.cfg uucico\fossil.c 
	$(CC) -c uucico\fossil.c

getopt.obj: uucico.cfg lib\getopt.c 
	$(CC) -c lib\getopt.c

hlib.obj: uucico.cfg lib\hlib.c 
	$(CC) -c lib\hlib.c

hostable.obj: uucico.cfg lib\hostable.c 
	$(CC) -c lib\hostable.c

hostatus.obj: uucico.cfg lib\hostatus.c 
	$(CC) -c lib\hostatus.c

import.obj: uucico.cfg lib\import.c 
	$(CC) -c lib\import.c

lib.obj: uucico.cfg lib\lib.c 
	$(CC) -c lib\lib.c

lock.obj: uucico.cfg lib\lock.c 
	$(CC) -c lib\lock.c

modem.obj: uucico.cfg uucico\modem.c 
	$(CC) -c uucico\modem.c

mx5.obj: uucico.cfg uucico\mx5.c 
	$(CC) -c uucico\mx5.c

pushpop.obj: uucico.cfg lib\pushpop.c 
	$(CC) -c lib\pushpop.c

screen.obj: uucico.cfg uucico\screen.c 
	$(CC) -c uucico\screen.c

script.obj: uucico.cfg uucico\script.c 
	$(CC) -c uucico\script.c

ssleep.obj: uucico.cfg lib\ssleep.c 
	$(CC) -c lib\ssleep.c

stater.obj: uucico.cfg lib\stater.c 
	$(CC) -c lib\stater.c

strpool.obj: uucico.cfg lib\strpool.c 
	$(CC) -c lib\strpool.c

swap.obj: uucico.cfg lib\swap.c 
	$(CC) -c lib\swap.c

timestmp.obj: uucico.cfg lib\timestmp.c 
	$(CC) -c lib\timestmp.c

tzset.obj: uucico.cfg lib\tzset.c 
	$(CC) -c lib\tzset.c

ulib.obj: uucico.cfg uucico\ulib.c 
	$(CC) -c uucico\ulib.c

usertabl.obj: uucico.cfg lib\usertabl.c 
	$(CC) -c lib\usertabl.c

commfifo.obj: uucico.cfg uucico\commfifo.asm 
	$(TASM) /MX /OS /W2 UUCICO\COMMFIFO.ASM,UUCICO\COMMFIFO.OBJ

#		*Compiler Configuration File*
uucico.cfg: uucico.mak
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
-wpin
-wamb
-wamp
-wasm
-wpro
-wdef
-w-sus
-wstv
-wuse
-weas
-wpre
-nUUCICO
-I$(INCLUDEPATH)
-L$(LIBPATH)
-P-.C
-v
-y
| uucico.cfg


