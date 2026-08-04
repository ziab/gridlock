# Gridlock Dev Runner — Rebuilds JUCE Standalone App & Flutter Companion App and runs them in parallel.

$ErrorActionPreference = "Stop"

Write-Host "======================================================" -ForegroundColor Cyan
Write-Host " Building Gridlock Standalone App (JUCE C++) ... " -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan

# 1. Configure and Build JUCE Standalone app
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --target MidiGridAnalyzer_Standalone

$juceExe = "build\MidiGridAnalyzer_artefacts\Debug\Standalone\MIDI Grid Analyzer.exe"

if (-not (Test-Path $juceExe)) {
    Write-Error "JUCE Standalone executable not found at: $juceExe"
}

Write-Host "`n======================================================" -ForegroundColor Green
Write-Host " Building Gridlock Companion App (Flutter Windows) ... " -ForegroundColor Green
Write-Host "======================================================" -ForegroundColor Green

# 2. Build Flutter Companion Windows Desktop app
Push-Location companion
try {
    flutter build windows --debug
} finally {
    Pop-Location
}

$flutterExe = "companion\build\windows\x64\runner\Debug\gridlock_companion.exe"

if (-not (Test-Path $flutterExe)) {
    Write-Error "Flutter Companion executable not found at: $flutterExe"
}

Write-Host "`n======================================================" -ForegroundColor Yellow
Write-Host " Launching JUCE App & Companion App in parallel ... " -ForegroundColor Yellow
Write-Host "======================================================" -ForegroundColor Yellow

# 3. Launch both applications in parallel
Write-Host "Starting JUCE Standalone App: $juceExe" -ForegroundColor Cyan
Start-Process -FilePath $juceExe

Write-Host "Starting Flutter Companion App: $flutterExe" -ForegroundColor Green
Start-Process -FilePath $flutterExe

Write-Host "`nBoth applications launched successfully!" -ForegroundColor Magenta
