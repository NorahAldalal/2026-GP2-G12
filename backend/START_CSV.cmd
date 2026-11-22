@echo off
setlocal
cd /d "%~dp0"
if not exist ".venv312\Scripts\python.exe" (
  echo Run SETUP_WINDOWS.cmd first.
  pause
  exit /b 1
)
set /p "SIRAJ_API_DIR=Paste the full folder path containing update_lamp.php: "
set "SIRAJ_API_DIR=%SIRAJ_API_DIR:"=%"
if not exist "%SIRAJ_API_DIR%\update_lamp.php" (
  echo update_lamp.php not found.
  pause
  exit /b 1
)
set "SIRAJ_READINGS_CSV=%SIRAJ_API_DIR%\siraj_readings.csv"
echo Model: siraj-cleaned782-synthetic134-v1
echo CSV: %SIRAJ_READINGS_CSV%
echo Keep this window open. Health: http://127.0.0.1:8765/health
".venv312\Scripts\python.exe" server.py
pause
