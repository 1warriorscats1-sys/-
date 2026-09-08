@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 (
  where python >nul 2>nul
  if errorlevel 1 (
    echo Install Python 3.10 or newer from python.org and enable Add Python to PATH.
    pause
    exit /b 1
  )
  python scripts\prepare_sd.py %*
) else (
  py -3 scripts\prepare_sd.py %*
)
set "result=%errorlevel%"
echo.
pause
exit /b %result%
