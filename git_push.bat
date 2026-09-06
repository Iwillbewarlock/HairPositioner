@echo off
setlocal
rem ---------------------------------------------------------------
rem  HairPositioner -- upload to GitHub (run from this folder)
rem ---------------------------------------------------------------
cd /d "%~dp0"

where git >nul 2>&1
if errorlevel 1 ( echo git not found. & pause & exit /b 1 )

if not exist ".git" git init
git checkout -B main >nul 2>&1

rem git refuses to commit without an identity -- set one for this repo only
git config user.name >nul 2>&1 || git config user.name "Iwillbewarlock"
git config user.email >nul 2>&1 || git config user.email "Iwillbewarlock@users.noreply.github.com"

git add -A
git commit -m "HairPositioner v1.0 -- hair position/rotation/scale sliders for RaceMenu, all races"
git rev-parse --verify HEAD >nul 2>&1
if errorlevel 1 (
  echo.
  echo No commit was created -- see the message above.
  pause & exit /b 1
)

set REPO=https://github.com/Iwillbewarlock/HairPositioner.git
git remote remove origin >nul 2>&1
git remote add origin "%REPO%"

rem the GitHub repo may already hold a README / LICENSE created on the site:
rem merge it in first (our files win on conflict), then push
git fetch origin main >nul 2>&1
if not errorlevel 1 (
  git merge --allow-unrelated-histories -X ours -m "merge GitHub-created files" origin/main
)
git push -u origin main
if errorlevel 1 (
  echo.
  echo push failed. If GitHub asked for a password, use a Personal Access Token,
  echo or sign in once with:  gh auth login
  pause & exit /b 1
)
echo.
echo done: %REPO%
pause
