@echo off
if /i "%1"=="both" (
    powershell -ExecutionPolicy Bypass -File "%~dp0run_app.ps1" -Both
) else if /i "%1"=="-both" (
    powershell -ExecutionPolicy Bypass -File "%~dp0run_app.ps1" -Both
) else (
    powershell -ExecutionPolicy Bypass -File "%~dp0run_app.ps1" %*
)
