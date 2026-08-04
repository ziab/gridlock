# Gridlock Release Runner — Rebuilds and launches Gridlock Standalone App (and optionally Companion App).
# Usage:
#   .\run_app.ps1          (Builds & launches JUCE Standalone App only)
#   .\run_app.ps1 -Both    (Builds & launches both JUCE Standalone App AND Flutter Companion App)

param(
    [switch]$Both
)

$ErrorActionPreference = "Stop"

Write-Host "======================================================" -ForegroundColor Cyan
Write-Host " Building Gridlock Standalone App (JUCE C++ Release) ... " -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan

# 1. Configure and Build JUCE Standalone App in Release mode
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target MidiGridAnalyzer_Standalone

$juceExe = "build\MidiGridAnalyzer_artefacts\Release\Standalone\MIDI Grid Analyzer.exe"

if (-not (Test-Path $juceExe)) {
    Write-Error "JUCE Standalone executable not found at: $juceExe"
}

$flutterExe = ""

if ($Both) {
    Write-Host "`n======================================================" -ForegroundColor Green
    Write-Host " Building Gridlock Companion App (Flutter Windows Release) ... " -ForegroundColor Green
    Write-Host "======================================================" -ForegroundColor Green

    # 2. Build Flutter Companion Windows Desktop app in Release mode
    Push-Location companion
    try {
        flutter build windows
    } finally {
        Pop-Location
    }

    $flutterExe = "companion\build\windows\x64\runner\Release\gridlock_companion.exe"

    if (-not (Test-Path $flutterExe)) {
        Write-Error "Flutter Companion executable not found at: $flutterExe"
    }
}

Write-Host "`n======================================================" -ForegroundColor Yellow
Write-Host " Launching Application(s) ... " -ForegroundColor Yellow
Write-Host "======================================================" -ForegroundColor Yellow

# 3. Launch selected application(s)
Write-Host "Starting JUCE Standalone App: $juceExe" -ForegroundColor Cyan
Start-Process -FilePath $juceExe

if ($Both -and $flutterExe -ne "") {
    Write-Host "Starting Flutter Companion App: $flutterExe" -ForegroundColor Green
    Start-Process -FilePath $flutterExe
}

Write-Host "`nApplication launch sequence complete!" -ForegroundColor Magenta
