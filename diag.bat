@echo off
cd /d "%~dp0"
(
  echo === HairPositioner diagnostic ===
  echo.
  echo --- cwd
  cd
  echo.
  echo --- VCPKG_ROOT
  echo [%VCPKG_ROOT%]
  echo.
  echo --- git
  git --version
  echo git exit=%errorlevel%
  echo.
  echo --- cmake
  cmake --version
  echo cmake exit=%errorlevel%
  echo.
  echo --- CommonLibSSE-NG present?
  if exist "extern\CommonLibSSE-NG\CMakeLists.txt" (echo YES) else (echo NO)
  dir /b extern\CommonLibSSE-NG
  echo.
  echo --- NG include sanity
  if exist "extern\CommonLibSSE-NG\include\RE\Skyrim.h" (echo Skyrim.h YES) else (echo Skyrim.h NO)
  if exist "extern\CommonLibSSE-NG\cmake\CommonLibSSE.cmake" (echo CommonLibSSE.cmake YES) else (echo CommonLibSSE.cmake NO)
  echo.
  echo --- visual studio instances
  "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -all -prerelease -products * -format value -property installationPath
  echo vswhere exit=%errorlevel%
  echo.
  echo --- cmake known VS generators
  cmake --help ^| findstr /C:"Visual Studio"
  echo.
  echo --- project files
  dir /b
) > diag_log.txt 2>&1

type diag_log.txt
echo.
echo ============================================
echo  Saved to diag_log.txt
echo ============================================
pause
