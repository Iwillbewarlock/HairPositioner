@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
set LOG=%~dp0compile.log
echo === HairPositioner papyrus compile === > "%LOG%"
echo %DATE% %TIME% >> "%LOG%"

rem NOTE: this project may live under a folder whose name contains parentheses.
rem Those parentheses close a cmd if(...) block early when a variable holding
rem them is expanded inside one, which silently kills the script. So every
rem branch below uses "if ... goto label" instead of parenthesised blocks, and
rem every echoed path is quoted.

set COMPILER=D:\TAKEALOOK\Stock Game\Papyrus Compiler\PapyrusCompiler.exe
set CKZIP=D:\TAKEALOOK\mods\Creation Kit\Root\Data\Scripts.zip
set SKSESRC=D:\TAKEALOOK\mods\Skyrim Script Extender (SKSE64)\Scripts\Source
set VANILLA=%~dp0papyrus\import\vanilla
set RMSRC=%~dp0papyrus\import\racemenu
set MYSRC=%~dp0papyrus\source
set OUT=%~dp0package\Scripts

if not exist "%COMPILER%" goto :nocompiler
if not exist "%SKSESRC%" goto :noskse
if not exist "%RMSRC%" goto :normsrc
if not exist "%MYSRC%" goto :nomysrc

rem ---- one-time: unpack the vanilla script sources from the CK's Scripts.zip
if exist "%VANILLA%" goto :haveVanilla
if not exist "%CKZIP%" goto :nockzip
echo unpacking vanilla script sources (this takes a moment) ... >> "%LOG%"
echo unpacking vanilla script sources (this takes a moment) ...
mkdir "%VANILLA%" 2>nul
powershell -NoProfile -Command "Expand-Archive -LiteralPath '%CKZIP%' -DestinationPath '%VANILLA%' -Force" >> "%LOG%" 2>&1
if errorlevel 1 goto :unzipfailed
:haveVanilla

rem ---- the flags file lives with the vanilla sources, wherever the zip put them
set FLAGDIR=
for /r "%VANILLA%" %%F in (TESV_Papyrus_Flags.flg) do if exist "%%F" set FLAGDIR=%%~dpF
if "%FLAGDIR%"=="" goto :noflags
if "%FLAGDIR:~-1%"=="\" set FLAGDIR=%FLAGDIR:~0,-1%

set IMPORTS=%MYSRC%;%RMSRC%;%SKSESRC%;%FLAGDIR%
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%~dp0package\Scripts\Source" mkdir "%~dp0package\Scripts\Source"

echo. >> "%LOG%"
echo compiler : "%COMPILER%" >> "%LOG%"
echo imports  : "%IMPORTS%" >> "%LOG%"
echo output   : "%OUT%" >> "%LOG%"

echo. >> "%LOG%"
if exist "%OUT%\HairPositionerNative.pex" del /q "%OUT%\HairPositionerNative.pex"
echo --- compiling HairPositioner --- >> "%LOG%"
echo compiling HairPositioner ...
"%COMPILER%" "HairPositioner" -f="TESV_Papyrus_Flags.flg" -i="%IMPORTS%" -o="%OUT%" >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo. >> "%LOG%"
echo --- compiling RaceMenuHairPositioner --- >> "%LOG%"
echo compiling RaceMenuHairPositioner ...
"%COMPILER%" "RaceMenuHairPositioner" -f="TESV_Papyrus_Flags.flg" -i="%IMPORTS%" -o="%OUT%" >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

copy /y "%MYSRC%\*.psc" "%~dp0package\Scripts\Source\" >> "%LOG%" 2>&1

echo. >> "%LOG%"
echo OK >> "%LOG%"
dir "%OUT%\*.pex" >> "%LOG%" 2>&1
echo.
echo DONE. See compile.log
pause
exit /b 0

:nocompiler
echo ERROR: PapyrusCompiler.exe not found at: >> "%LOG%"
echo   "%COMPILER%" >> "%LOG%"
goto :fail

:noskse
echo ERROR: SKSE script sources not found at: >> "%LOG%"
echo   "%SKSESRC%" >> "%LOG%"
goto :fail

:normsrc
echo ERROR: RaceMenu script sources not found at: >> "%LOG%"
echo   "%RMSRC%" >> "%LOG%"
goto :fail

:nomysrc
echo ERROR: own script sources not found at: >> "%LOG%"
echo   "%MYSRC%" >> "%LOG%"
goto :fail

:nockzip
echo ERROR: Scripts.zip not found at: >> "%LOG%"
echo   "%CKZIP%" >> "%LOG%"
goto :fail

:unzipfailed
echo ERROR: Expand-Archive failed >> "%LOG%"
goto :fail

:noflags
echo ERROR: TESV_Papyrus_Flags.flg not found under: >> "%LOG%"
echo   "%VANILLA%" >> "%LOG%"
echo   the Scripts.zip layout was not what was expected >> "%LOG%"
goto :fail

:fail
echo. >> "%LOG%"
echo FAILED >> "%LOG%"
echo.
echo FAILED. See compile.log
pause
exit /b 1
