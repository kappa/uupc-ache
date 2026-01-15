#		*Translator Definitions*
WATCOM = d:\watcom
CC = $(WATCOM)\binb\wcc386.exe
LINK = $(WATCOM)\binb\wcl386.exe
LIBPATH = $(WATCOM)\LIB;
TCP_INCLUDE = d:\watcom\include
MODEL =
COPT = /i=$(TCP_INCLUDE);LIB /zp1 /w3 /oxt /bm $(MODEL)
LOPT = /x /k64k $(MODEL)

!ifdef DEBUG
CFLAGS = $(COPT) /d2 /hw
LFLAGS = $(LOPT) /d2 /hw
OBJDIR = obj2\debug
LXLITE = @rem
!else
CFLAGS = $(COPT)
LFLAGS = $(LOPT)
OBJDIR = obj2
LXLITE = lxlite
!endif

#		*Implicit Rules*

.DEFAULT
  $(CC) $(CFLAGS) /fo=$@ $[@


All: batch rmail testexp testimp uucico uucp uupoll uustat uusub uux uuxqt .SYMBOLIC
  @rem 

batch:      $(OBJDIR)\batch.exe .SYMBOLIC
  @rem 
rmail:      $(OBJDIR)\rmail.exe .SYMBOLIC
  @rem 
testexp:    $(OBJDIR)\testexp.exe .SYMBOLIC
  @rem 
testimp:    $(OBJDIR)\testimp.exe .SYMBOLIC
  @rem 
uucico:     $(OBJDIR)\uucico.exe .SYMBOLIC
  @rem 
uucp:       $(OBJDIR)\uucp.exe .SYMBOLIC
  @rem 
uupoll:     $(OBJDIR)\uupoll.exe .SYMBOLIC
  @rem 
uustat:     $(OBJDIR)\uustat.exe .SYMBOLIC
  @rem 
uusub:      $(OBJDIR)\uusub.exe .SYMBOLIC
  @rem 
uux:        $(OBJDIR)\uux.exe .SYMBOLIC
  @rem 
uuxqt:      $(OBJDIR)\uuxqt.exe .SYMBOLIC
  @rem 

OBJS = &
 $(OBJDIR)\arbmath.obj &
 $(OBJDIR)\chdir.obj &
 $(OBJDIR)\dater.obj &
 $(OBJDIR)\flock.obj &
 $(OBJDIR)\getopt.obj &
 $(OBJDIR)\hlib.obj &
 $(OBJDIR)\hostable.obj &
 $(OBJDIR)\import.obj &
 $(OBJDIR)\lib.obj &
 $(OBJDIR)\strpool.obj &
 $(OBJDIR)\timestmp.obj &
 $(OBJDIR)\tzset.obj

$(OBJDIR)\batch.exe: &
 $(OBJS) &
 $(OBJDIR)\batch.obj &
 $(OBJDIR)\arpadate.obj &
 $(OBJDIR)\cp.obj &
 $(OBJDIR)\expath.obj &
 $(OBJDIR)\fakeport.obj &
 $(OBJDIR)\fakescr.obj &
 $(OBJDIR)\fakeslep.obj &
 $(OBJDIR)\getseq.obj &
 $(OBJDIR)\hostatus.obj &
 $(OBJDIR)\lock.obj &
 $(OBJDIR)\pushpop.obj &
 $(OBJDIR)\stater.obj &
 $(OBJDIR)\swap2.obj &
 $(OBJDIR)\usertabl.obj &
 $(OBJDIR)\os2conio.obj
	$(LINK) $(LFLAGS) /fe=$@ $<
	$(LXLITE) /D+ /MLN /MRN /Tobj\batch.exe $@

$(OBJDIR)\rmail.exe: &
 $(OBJS) &
 $(OBJDIR)\rmail.obj &
 $(OBJDIR)\address.obj &
 $(OBJDIR)\arpadate.obj &
 $(OBJDIR)\fakeport.obj &
 $(OBJDIR)\fakescr.obj &
 $(OBJDIR)\getseq.obj &
 $(OBJDIR)\head.obj &
 $(OBJDIR)\koi8.obj &
 $(OBJDIR)\mime.obj &
 $(OBJDIR)\os2conio.obj &
 $(OBJDIR)\pcmail.obj &
 $(OBJDIR)\sendone.obj &
 $(OBJDIR)\ssleep2.obj &
 $(OBJDIR)\stat.obj &
 $(OBJDIR)\swap2.obj &
 $(OBJDIR)\tio.obj &
 $(OBJDIR)\usertabl.obj
	$(LINK) $(LFLAGS) /fe=$@ $<
	$(LXLITE) /D+ /MLN /MRN /Tuucico\rmail.exe $@

$(OBJDIR)\testexp.exe: &
 $(OBJS) &
 $(OBJDIR)\testexp.obj &
 $(OBJDIR)\export.obj &
 $(OBJDIR)\fakescr.obj &
 $(OBJDIR)\fakeslep.obj &
 $(OBJDIR)\os2conio.obj
	$(LINK) $(LFLAGS) /fe=$@ $<
	$(LXLITE) /D+ /MLN /MRN /Tobj\testexp.exe $@

$(OBJDIR)\testimp.exe: &
 $(OBJS) &
 $(OBJDIR)\testimp.obj &
 $(OBJDIR)\export.obj &
 $(OBJDIR)\fakescr.obj &
 $(OBJDIR)\fakeslep.obj &
 $(OBJDIR)\os2conio.obj
	$(LINK) $(LFLAGS) /fe=$@ $<
	$(LXLITE) /D+ /MLN /MRN /Tobj\testimp.exe $@

$(OBJDIR)\uucico.exe: &
 $(OBJS) &
 $(OBJDIR)\uucico.obj &
 $(OBJDIR)\arpadate.obj &
 $(OBJDIR)\checktim.obj &
 $(OBJDIR)\dcp.obj &
 $(OBJDIR)\dcpgpkt.obj &
 $(OBJDIR)\dcplib.obj &
 $(OBJDIR)\dcpstats.obj &
 $(OBJDIR)\dcpsys.obj &
 $(OBJDIR)\dcptpkt.obj &
 $(OBJDIR)\dcpxfer.obj &
 $(OBJDIR)\expath.obj &
 $(OBJDIR)\hostatus.obj &
 $(OBJDIR)\loadtcp.obj &
 $(OBJDIR)\lock.obj &
 $(OBJDIR)\modem.obj &
 $(OBJDIR)\os2conio.obj &
 $(OBJDIR)\pushpop.obj &
 $(OBJDIR)\screen.obj &
 $(OBJDIR)\script.obj &
 $(OBJDIR)\sio.obj &
 $(OBJDIR)\ssleep2.obj &
 $(OBJDIR)\stater.obj &
 $(OBJDIR)\swap2.obj &
 $(OBJDIR)\ulib2.obj &
 $(OBJDIR)\usertabl.obj
	$(LINK) $(LFLAGS) /fe=$@ $<
	$(LXLITE) /D+ /ML1 /Tuucico\uucico.exe $@

$(OBJDIR)\uucp.exe: &
 $(OBJS) &
 $(OBJDIR)\uucp.obj &
 $(OBJDIR)\cp.obj &
 $(OBJDIR)\expath.obj &
 $(OBJDIR)\fakeport.obj &
 $(OBJDIR)\fakescr.obj &
 $(OBJDIR)\fakeslep.obj &
 $(OBJDIR)\getseq.obj &
 $(OBJDIR)\normaliz.obj &
 $(OBJDIR)\os2conio.obj &
 $(OBJDIR)\pushpop.obj &
 $(OBJDIR)\swap2.obj &
 $(OBJDIR)\usertabl.obj
	$(LINK) $(LFLAGS) /fe=$@ $<
	$(LXLITE) /D+ /ML1 /Tobj\uucp.exe $@

$(OBJDIR)\uupoll.exe: &
 $(OBJS) &
 $(OBJDIR)\uupoll.obj &
 $(OBJDIR)\arpadate.obj &
 $(OBJDIR)\fakeport.obj &
 $(OBJDIR)\fakescr.obj &
 $(OBJDIR)\os2conio.obj &
 $(OBJDIR)\ssleep2.obj &
 $(OBJDIR)\swap2.obj
	$(LINK) $(LFLAGS) /fe=$@ $<
	$(LXLITE) /D+ /ML1 /Tobj\uupoll.exe $@

$(OBJDIR)\uustat.exe: &
 $(OBJS) &
 $(OBJDIR)\uustat.obj &
 $(OBJDIR)\export.obj &
 $(OBJDIR)\fakeport.obj &
 $(OBJDIR)\fakescr.obj &
 $(OBJDIR)\fakeslep.obj &
 $(OBJDIR)\getseq.obj &
 $(OBJDIR)\hostatus.obj &
 $(OBJDIR)\os2conio.obj &
 $(OBJDIR)\pushpop.obj &
 $(OBJDIR)\readnext.obj &
 $(OBJDIR)\stater.obj
	$(LINK) $(LFLAGS) /fe=$@ $<
	$(LXLITE) /D+ /ML1 /Tobj\uustat.exe $@

$(OBJDIR)\uusub.exe: &
 $(OBJS) &
 $(OBJDIR)\uusub.obj &
 $(OBJDIR)\fakeport.obj &
 $(OBJDIR)\fakescr.obj &
 $(OBJDIR)\fakeslep.obj &
 $(OBJDIR)\hostrset.obj &
 $(OBJDIR)\hostatus.obj &
 $(OBJDIR)\lock.obj &
 $(OBJDIR)\os2conio.obj &
 $(OBJDIR)\stater.obj
	$(LINK) $(LFLAGS) /fe=$@ $<
	$(LXLITE) /D+ /ML1 /Tobj\uusub.exe $@

$(OBJDIR)\uux.exe: &
 $(OBJS) &
 $(OBJDIR)\uux.obj &
 $(OBJDIR)\arpadate.obj &
 $(OBJDIR)\cp.obj &
 $(OBJDIR)\expath.obj &
 $(OBJDIR)\fakeport.obj &
 $(OBJDIR)\fakescr.obj &
 $(OBJDIR)\fakeslep.obj &
 $(OBJDIR)\getseq.obj &
 $(OBJDIR)\hostatus.obj &
 $(OBJDIR)\os2conio.obj &
 $(OBJDIR)\pushpop.obj &
 $(OBJDIR)\stater.obj &
 $(OBJDIR)\swap2.obj &
 $(OBJDIR)\usertabl.obj
	$(LINK) $(LFLAGS) /fe=$@ $<
	$(LXLITE) /D+ /MLN /MRN /Tobj\uux.exe $@

$(OBJDIR)\uuxqt.exe: &
 $(OBJS) &
 $(OBJDIR)\uuxqt.obj &
 $(OBJDIR)\arpadate.obj &
 $(OBJDIR)\cp.obj &
 $(OBJDIR)\expath.obj &
 $(OBJDIR)\fakeport.obj &
 $(OBJDIR)\fakeslep.obj &
 $(OBJDIR)\koi8.obj &
 $(OBJDIR)\lock.obj &
 $(OBJDIR)\os2conio.obj &
 $(OBJDIR)\pushpop.obj &
 $(OBJDIR)\readnext.obj &
 $(OBJDIR)\screen.obj &
 $(OBJDIR)\stater.obj &
 $(OBJDIR)\swap2.obj &
 $(OBJDIR)\usertabl.obj
	$(LINK) $(LFLAGS) /fe=$@ $<
	$(LXLITE) /D+ /ML1 /Tuucico\uuxqt.exe $@

#		*Individual File Dependencies*
$(OBJDIR)\batch.obj: util\batch.c .AUTODEPEND
$(OBJDIR)\rmail.obj: rmail\rmail.c .AUTODEPEND
$(OBJDIR)\address.obj: rmail\address.c .AUTODEPEND
$(OBJDIR)\arbmath.obj: lib\arbmath.c .AUTODEPEND
$(OBJDIR)\arpadate.obj: lib\arpadate.c .AUTODEPEND
$(OBJDIR)\chdir.obj: lib\chdir.c .AUTODEPEND
$(OBJDIR)\checktim.obj: uucico\checktim.c .AUTODEPEND
$(OBJDIR)\cp.obj: lib\cp.c .AUTODEPEND
$(OBJDIR)\dater.obj: lib\dater.c .AUTODEPEND
$(OBJDIR)\dcp.obj: uucico\dcp.c .AUTODEPEND
$(OBJDIR)\dcpgpkt.obj: uucico\dcpgpkt.c .AUTODEPEND
$(OBJDIR)\dcplib.obj: uucico\dcplib.c .AUTODEPEND
$(OBJDIR)\dcpstats.obj: uucico\dcpstats.c .AUTODEPEND
$(OBJDIR)\dcpsys.obj: uucico\dcpsys.c .AUTODEPEND
$(OBJDIR)\dcptpkt.obj: uucico\dcptpkt.c .AUTODEPEND
$(OBJDIR)\dcpxfer.obj: uucico\dcpxfer.c .AUTODEPEND
$(OBJDIR)\expath.obj: lib\expath.c .AUTODEPEND
$(OBJDIR)\export.obj: lib\export.c .AUTODEPEND
$(OBJDIR)\fakeport.obj: lib\fakeport.c .AUTODEPEND
$(OBJDIR)\fakescr.obj: lib\fakescr.c .AUTODEPEND
$(OBJDIR)\fakeslep.obj: lib\fakeslep.c .AUTODEPEND
$(OBJDIR)\flock.obj: lib\flock.c .AUTODEPEND
$(OBJDIR)\getopt.obj: lib\getopt.c .AUTODEPEND
$(OBJDIR)\getseq.obj: lib\getseq.c .AUTODEPEND
$(OBJDIR)\head.obj: rmail\head.c .AUTODEPEND
$(OBJDIR)\hlib.obj: lib\hlib.c .AUTODEPEND
$(OBJDIR)\hostable.obj: lib\hostable.c .AUTODEPEND
$(OBJDIR)\hostatus.obj: lib\hostatus.c .AUTODEPEND
$(OBJDIR)\hostrset.obj: lib\hostrset.c .AUTODEPEND
$(OBJDIR)\import.obj: lib\import.c .AUTODEPEND
$(OBJDIR)\koi8.obj: rmail\koi8.c .AUTODEPEND
$(OBJDIR)\lib.obj: lib\lib.c .AUTODEPEND
$(OBJDIR)\loadtcp.obj: uucico\loadtcp.c .AUTODEPEND
$(OBJDIR)\lock.obj: lib\lock.c .AUTODEPEND
$(OBJDIR)\mime.obj: rmail\mime.c .AUTODEPEND
$(OBJDIR)\modem.obj: uucico\modem.c .AUTODEPEND
$(OBJDIR)\normaliz.obj: lib\normaliz.c .AUTODEPEND
$(OBJDIR)\os2conio.obj: lib\os2conio.c .AUTODEPEND
$(OBJDIR)\pcmail.obj: rmail\pcmail.c .AUTODEPEND
$(OBJDIR)\pushpop.obj: lib\pushpop.c .AUTODEPEND
$(OBJDIR)\readnext.obj: lib\readnext.c .AUTODEPEND
$(OBJDIR)\screen.obj: uucico\screen.c .AUTODEPEND
$(OBJDIR)\script.obj: uucico\script.c .AUTODEPEND
$(OBJDIR)\sendone.obj: rmail\sendone.c .AUTODEPEND
$(OBJDIR)\sio.obj: uucico\sio.c .AUTODEPEND
$(OBJDIR)\ssleep2.obj: lib\ssleep2.c .AUTODEPEND
$(OBJDIR)\stat.obj: rmail\stat.c .AUTODEPEND
$(OBJDIR)\stater.obj: lib\stater.c .AUTODEPEND
$(OBJDIR)\strpool.obj: lib\strpool.c .AUTODEPEND
$(OBJDIR)\swap2.obj: lib\swap2.c .AUTODEPEND
$(OBJDIR)\testexp.obj: util\testexp.c .AUTODEPEND
$(OBJDIR)\testimp.obj: util\testimp.c .AUTODEPEND
$(OBJDIR)\timestmp.obj: lib\timestmp.c .AUTODEPEND
$(OBJDIR)\tio.obj: rmail\tio.c .AUTODEPEND
$(OBJDIR)\tzset.obj: lib\tzset.c .AUTODEPEND
$(OBJDIR)\uucp.obj: util\uucp.c .AUTODEPEND
$(OBJDIR)\ulib2.obj: uucico\ulib2.c .AUTODEPEND
$(OBJDIR)\usertabl.obj: lib\usertabl.c .AUTODEPEND
$(OBJDIR)\uucico.obj: uucico\uucico.c .AUTODEPEND
$(OBJDIR)\uupoll.obj: util\uupoll.c .AUTODEPEND
$(OBJDIR)\uustat.obj: util\uustat.c .AUTODEPEND
$(OBJDIR)\uusub.obj: util\uusub.c .AUTODEPEND
$(OBJDIR)\uux.obj: util\uux.c .AUTODEPEND
$(OBJDIR)\uuxqt.obj: util\uuxqt.c .AUTODEPEND
