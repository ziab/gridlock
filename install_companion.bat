@echo off
REM Gridlock Companion Installer — Batch wrapper for install_companion.ps1
REM Usage: install_companion.bat [-DeviceId <serial>] [-NoBuild] [-ListDevices]

powershell -ExecutionPolicy Bypass -File "%~dp0install_companion.ps1" %*
