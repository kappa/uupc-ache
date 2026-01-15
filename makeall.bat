@rem Borland C buid
make -f batch.mak
call compr.bat obj\batch.exe
make -f rmail.mak
call compr.bat uucico\rmail.exe
make -f uucico.mak
call compr.bat uucico\uucico.exe
make -f uucp.mak
call compr.bat obj\uucp.exe
make -f uupoll.mak
call compr.bat obj\uupoll.exe
make -f uustat.mak
call compr.bat obj\uustat.exe
make -f uusub.mak
call compr.bat obj\uusub.exe
make -f uux.mak
call compr.bat obj\uux.exe
make -f uuxqt.mak
call compr.bat uucico\uuxqt.exe
make -f testimp.mak
call compr.bat obj\testimp.exe
make -f testexp.mak
call compr.bat obj\testexp.exe

