@echo off
cd /d "%~dp0"

(
  echo === setup ===
  if exist "extern\CommonLibSSE-NG\CMakeLists.txt" (
    echo CommonLibSSE-NG already present, skipping clone.
  ) else (
    echo Cloning CommonLibSSE-NG into extern\ ...
    git clone --depth 1 https://github.com/CharmedBaryon/CommonLibSSE-NG.git extern\CommonLibSSE-NG
    echo git clone exit=%errorlevel%
  )
  echo.
  echo --- result
  if exist "extern\CommonLibSSE-NG\CMakeLists.txt" (echo OK) else (echo FAILED)
) > setup_log.txt 2>&1

type setup_log.txt
echo.
echo  Saved to setup_log.txt
pause
