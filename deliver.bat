@echo off
copy "Windows Mobile 6 Professional SDK (ARMV4I)\Release\APRSISCE6P.exe" . /y
copy aprsis32\debug\APRSIS32.exe . /y
copy aprsis32\debug\APRSIS32.pdb . /y
rem symstore add /f DebuggingSeries.* /s \\camerons4\Symbols\MySymbols /t "My Version 1" /v "1.0.0.0" /c "Manually adding"
copy CE5x86Rel\APRSISCE5x86.exe . /y
u:
if /i "%1" == "CE5" goto CE5
@rem
@rem
cd \aprsis32
copy c:APRSIS32.exe . /y
crcit APRSIS32.exe
rem symstore add /f APRSIS32.* /s u:\aprsis32\symstore /t "Test Version 1" /v "1.0.0.0" /c "Manually adding"
del APRSIS32.exe
@for %%d in (*.ver) do @set ver=%%d
copy %ver% APRSIS32.development /y >nul
if /i "%1" == "release" copy APRSIS32.development APRSIS32.release /y >nul
type APRSIS32.development
@rem
@rem
cd \aprsisce6p
copy c:APRSISCE6P.exe . /y
crcit APRSISCE6P.exe
del APRSISCE6P.exe
@for %%d in (*.ver) do @set ver=%%d
copy %ver% APRSISCE6P.development /y >nul
if /i "%1" == "release" copy APRSISCE6P.development APRSISCE6P.release /y >nul
type APRSISCE6P.development
@rem
@rem
:CE5
cd \aprsisce5x86
copy c:APRSISCE5x86.exe . /y
crcit APRSISCE5x86.exe
del APRSISCE5x86.exe
@for %%d in (*.ver) do @set ver=%%d
copy %ver% APRSISCE5x86.development /y >nul
if /i "%1" == "release" copy APRSISCE5x86.development APRSISCE5x86.release /y >nul
type APRSISCE5x86.development
@rem
@rem
c:

