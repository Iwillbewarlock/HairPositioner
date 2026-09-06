@echo off
cd /d "%~dp0"

if not exist "extern\CommonLibSSE-NG\CMakeLists.txt" (
    echo extern\CommonLibSSE-NG missing. Run setup.bat first.
    pause
    exit /b 1
)

if not defined VCPKG_ROOT (
    echo VCPKG_ROOT is not set.
    pause
    exit /b 1
)

echo [0/2] recording cmake generator list...
cmake --help > cmake_help_raw.txt 2>&1
findstr /C:"Visual Studio" cmake_help_raw.txt > cmake_generators.txt 2>&1
type cmake_generators.txt

echo.
echo [1/2] configuring...
cmake --preset default > cmake_log_latest.txt 2>&1
set CFG=%errorlevel%
type cmake_log_latest.txt
if not "%CFG%"=="0" (
    echo.
    echo  CMake configure FAILED ^(exit %CFG%^). See cmake_log_latest.txt
    pause
    exit /b 1
)

echo.
echo [2/2] building Release...
cmake --build build --config Release > build_log_latest.txt 2>&1
type build_log_latest.txt

echo.
echo ============================================
if exist "build\Release\HairPositioner.dll" (
    echo  OK: build\Release\HairPositioner.dll
    if not exist "package\SKSE\Plugins" mkdir "package\SKSE\Plugins"
    copy /y "build\Release\HairPositioner.dll" "package\SKSE\Plugins\" >nul
    echo  copied to package\SKSE\Plugins\HairPositioner.dll
) else (
    echo  HairPositioner.dll was NOT produced. Check build_log_latest.txt
)
echo ============================================
pause
