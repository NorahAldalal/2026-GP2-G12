@echo off
setlocal
cd /d "%~dp0"
py -3.12 -m venv .venv312
if errorlevel 1 goto fail
".venv312\Scripts\python.exe" -m pip install -r requirements.txt
if errorlevel 1 goto fail
".venv312\Scripts\python.exe" verify_model.py
if errorlevel 1 goto fail
echo Setup and model verification completed. Run START_CSV.cmd next.
pause
exit /b 0
:fail
echo Setup failed. Read the error above. Python 3.12 is required.
pause
exit /b 1
