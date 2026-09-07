@echo off
setlocal
rem ---------------------------------------------------------------
rem  HairPositioner -- first upload to GitHub (run from this folder)
rem  1) Create an EMPTY repository on GitHub (no README / .gitignore)
rem  2) Run this file and paste the repository URL when asked
rem ---------------------------------------------------------------
cd /d "%~dp0"

where git >nul 2>&1
if errorlevel 1 (
  echo git not found. Install Git for Windows first.
  pause & exit /b 1
)

if not exist ".git" git init -b main
git add -A
git commit -m "HairPositioner v1.0 -- hair position/rotation/scale sliders for RaceMenu, all races" >nul 2>&1
if errorlevel 1 echo (nothing new to commit)

set /p REPO=GitHub repository URL (e.g. https://github.com/USER/HairPositioner.git): 
if "%REPO%"=="" ( echo no URL given & pause & exit /b 1 )

git remote remove origin >nul 2>&1
git remote add origin "%REPO%"
git branch -M main
git push -u origin main
if errorlevel 1 (
  echo.
  echo push failed -- if GitHub asked for a password, use a Personal Access Token,
  echo or sign in once with:  gh auth login
  pause & exit /b 1
)
echo.
echo done: %REPO%
pause
