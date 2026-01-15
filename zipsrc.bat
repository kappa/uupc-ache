@echo off
set a=%1
if x%a% == x set a=a:
set name=sources.exe
set new=n
if not exist %a%%name% set new=y
lha u /r2x1m1l %a%%name% @zip.lst
if %new% == n goto l1
%a%
lha s /x1m1 %name%
:l1
