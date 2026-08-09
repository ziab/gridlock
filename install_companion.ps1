# Gridlock Companion Installer -- Builds release APK and installs to connected Android phone(s).
# Usage:
#   .\install_companion.ps1              # Auto-detect device, build release APK, install
#   .\install_companion.ps1 -DeviceId 51061FDAP001GZ   # Install to specific device
#   .\install_companion.ps1 -NoBuild     # Skip build, only install existing APK
#   .\install_companion.ps1 -ListDevices # Just list connected devices and exit

param(
    [string]$DeviceId = "",
    [switch]$NoBuild,
    [switch]$ListDevices
)

$ErrorActionPreference = "Stop"

function Test-CommandExists {
    param([string]$Name)
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Get-AndroidDevices {
    $output = adb devices 2>&1 | Out-String
    $lines = $output -split "`r?`n"
    $devices = @()
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        if ($trimmed -eq "" -or $trimmed.StartsWith("List of devices")) { continue }
        # Format: <serial> <state> [extra]
        $parts = $trimmed -split "\s+"
        if ($parts.Count -ge 2) {
            $serial = $parts[0]
            $state = $parts[1]
            $devices += [PSCustomObject]@{ Serial = $serial; State = $state; Raw = $trimmed }
        }
    }
    return $devices
}

# --- Pre-flight checks ---

if (-not (Test-CommandExists "adb")) {
    Write-Host "[ERROR] 'adb' not found in PATH." -ForegroundColor Red
    Write-Host "  Install Android Platform Tools or Android Studio and ensure adb is in PATH." -ForegroundColor Yellow
    Write-Host "  e.g. choco install adb  OR  via Android Studio > SDK Manager > Platform Tools" -ForegroundColor Yellow
    exit 1
}

if (-not $NoBuild -and -not (Test-CommandExists "flutter")) {
    Write-Host "[ERROR] 'flutter' not found in PATH." -ForegroundColor Red
    Write-Host "  Install Flutter SDK: https://docs.flutter.dev/get-started/install" -ForegroundColor Yellow
    exit 1
}

Write-Host "======================================================" -ForegroundColor Cyan
Write-Host " Gridlock Companion -- Release Installer " -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan

# --- Check connected devices ---

Write-Host "`nChecking for connected Android device(s)..." -ForegroundColor Yellow
$allDevices = Get-AndroidDevices

if ($ListDevices) {
    if ($allDevices.Count -eq 0) {
        Write-Host "  No devices found (adb devices returned empty)." -ForegroundColor Yellow
    } else {
        Write-Host "  adb devices output:" -ForegroundColor White
        foreach ($d in $allDevices) {
            $color = if ($d.State -eq "device") { "Green" } else { "Red" }
            Write-Host "    $($d.Serial)  $($d.State)" -ForegroundColor $color
            if ($d.State -ne "device") {
                Write-Host "      Raw: $($d.Raw)" -ForegroundColor DarkGray
            }
        }
    }
    # Also show flutter devices for cross-check
    Write-Host "`n  flutter devices:" -ForegroundColor White
    flutter devices 2>&1 | Write-Host
    exit 0
}

$readyDevices = @($allDevices | Where-Object { $_.State -eq "device" })
$problemDevices = @($allDevices | Where-Object { $_.State -ne "device" })

if ($problemDevices.Count -gt 0) {
    foreach ($d in $problemDevices) {
        if ($d.State -eq "unauthorized") {
            Write-Host "[WARN] Device $($d.Serial) is unauthorized." -ForegroundColor Red
            Write-Host "  -> On your phone: accept the 'Allow USB debugging?' prompt, enable USB Debugging in Developer Options." -ForegroundColor Yellow
        } elseif ($d.State -eq "offline") {
            Write-Host "[WARN] Device $($d.Serial) is offline. Try: adb kill-server; adb start-server; replug USB." -ForegroundColor Yellow
        } else {
            Write-Host "[WARN] Device $($d.Serial) state: $($d.State) ($($d.Raw))" -ForegroundColor Yellow
        }
    }
}

if ($readyDevices.Count -eq 0) {
    Write-Host "`n[ERROR] No authorized Android device connected." -ForegroundColor Red
    Write-Host "  Steps to connect your phone:" -ForegroundColor Yellow
    Write-Host "    1. Enable Developer Options: Settings > About phone > Tap 'Build number' 7x" -ForegroundColor White
    Write-Host "    2. Enable USB Debugging: Settings > Developer options > USB debugging = ON" -ForegroundColor White
    Write-Host "    3. Connect phone via USB cable (or Wireless debugging: adb pair / adb connect <ip>:<port>)" -ForegroundColor White
    Write-Host "    4. On phone, tap 'Allow' when prompted for USB debugging authorization" -ForegroundColor White
    Write-Host "    5. Verify with: adb devices  (should show '<id>  device')" -ForegroundColor White
    Write-Host "`n  Current adb output:" -ForegroundColor DarkGray
    adb devices 2>&1 | Write-Host
    exit 1
}

# Filter to target device(s)
$targetDevices = @()
if ($DeviceId -ne "") {
    $matched = @($readyDevices | Where-Object { $_.Serial -eq $DeviceId })
    if ($matched.Count -eq 0) {
        Write-Host "[ERROR] Device '$DeviceId' not found among ready devices." -ForegroundColor Red
        Write-Host "  Ready devices: $($readyDevices.Serial -join ', ')" -ForegroundColor Yellow
        exit 1
    }
    $targetDevices = $matched
    Write-Host "  Target device: $DeviceId (explicit)" -ForegroundColor Green
} else {
    $targetDevices = $readyDevices
    if ($targetDevices.Count -eq 1) {
        Write-Host "  Found device: $($targetDevices[0].Serial) (ready)" -ForegroundColor Green
    } else {
        Write-Host "  Found $($targetDevices.Count) ready devices: $($targetDevices.Serial -join ', ')" -ForegroundColor Green
        Write-Host "  -> Will install to ALL ready devices. Use -DeviceId <serial> to target one." -ForegroundColor Yellow
    }
}

# Show detailed device info
foreach ($d in $targetDevices) {
    try {
        $model = (adb -s $d.Serial shell getprop ro.product.model 2>$null).Trim()
        $androidVer = (adb -s $d.Serial shell getprop ro.build.version.release 2>$null).Trim()
        if ($model -ne "") {
            Write-Host "    - $($d.Serial)  $model  (Android $androidVer)" -ForegroundColor White
        }
    } catch {
        Write-Host "    - $($d.Serial)" -ForegroundColor White
    }
}

# --- Build release APK (optimized) ---

$apkPath = "companion\build\app\outputs\flutter-apk\app-release.apk"

if (-not $NoBuild) {
    Write-Host "`n======================================================" -ForegroundColor Green
    Write-Host " Building Companion App (Flutter Android Release) ... " -ForegroundColor Green
    Write-Host "======================================================" -ForegroundColor Green
    Write-Host "  This builds an optimized release APK (AOT + R8 minification, no debug overhead)." -ForegroundColor DarkGray
    Write-Host "  Command: flutter build apk --release" -ForegroundColor DarkGray

    Push-Location companion
    try {
        flutter build apk --release
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[ERROR] flutter build apk --release failed (exit $LASTEXITCODE)." -ForegroundColor Red
            exit $LASTEXITCODE
        }
    } finally {
        Pop-Location
    }

    if (-not (Test-Path $apkPath)) {
        Write-Host "[ERROR] Expected APK not found at: $apkPath" -ForegroundColor Red
        Write-Host "  Flutter build may have produced a different output. Searching..." -ForegroundColor Yellow
        $found = Get-ChildItem -Recurse -Filter "app-release.apk" -ErrorAction SilentlyContinue | Select-Object -First 3
        if ($found) {
            $found | ForEach-Object { Write-Host "    Found: $($_.FullName)" -ForegroundColor White }
        }
        exit 1
    }

    $apkSizeMb = [math]::Round((Get-Item $apkPath).Length / 1MB, 2)
    Write-Host "  Built: $apkPath ($apkSizeMb MB)" -ForegroundColor Green
} else {
    Write-Host "`n[INFO] -NoBuild specified, skipping build step." -ForegroundColor Yellow
    if (-not (Test-Path $apkPath)) {
        Write-Host "[ERROR] No existing APK at $apkPath. Remove -NoBuild to build first." -ForegroundColor Red
        exit 1
    }
    $apkSizeMb = [math]::Round((Get-Item $apkPath).Length / 1MB, 2)
    Write-Host "  Using existing APK: $apkPath ($apkSizeMb MB)" -ForegroundColor White
}

# --- Install to device(s) ---

Write-Host "`n======================================================" -ForegroundColor Yellow
Write-Host " Installing to phone(s) ... " -ForegroundColor Yellow
Write-Host "======================================================" -ForegroundColor Yellow

$failed = @()
foreach ($d in $targetDevices) {
    Write-Host "`nInstalling to $($d.Serial)..." -ForegroundColor Cyan
    # -r = reinstall/upgrade, keeps data; add -d to allow downgrade if needed
    adb -s $d.Serial install -r $apkPath
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [FAIL] adb install failed for $($d.Serial) (exit $LASTEXITCODE)" -ForegroundColor Red
        $failed += $d.Serial
    } else {
        Write-Host "  [OK] Installed to $($d.Serial)" -ForegroundColor Green
        # Verify package
        $pkgCheck = adb -s $d.Serial shell pm list packages com.gridlock.gridlock_companion 2>&1 | Out-String
        if ($pkgCheck -match "com.gridlock.gridlock_companion") {
            Write-Host "  Verified package: com.gridlock.gridlock_companion" -ForegroundColor DarkGray
            try {
                $ver = (adb -s $d.Serial shell dumpsys package com.gridlock.gridlock_companion 2>$null | Select-String "versionName").ToString().Trim()
                if ($ver -ne "") { Write-Host "  $ver" -ForegroundColor DarkGray }
            } catch {}
        }
        # Optionally launch the app (warning "brought to front" is normal if already running)
        Write-Host "  Launching app..." -ForegroundColor DarkGray
        adb -s $d.Serial shell am start -n com.gridlock.gridlock_companion/com.gridlock.gridlock_companion.MainActivity 2>$null | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  Note: am start returned $LASTEXITCODE (app may already be in foreground)" -ForegroundColor DarkGray
        }
    }
}

Write-Host "`n======================================================" -ForegroundColor Magenta
if ($failed.Count -eq 0) {
    Write-Host " Done! Companion app installed on $($targetDevices.Count) device(s)." -ForegroundColor Magenta
    Write-Host " You can now disconnect USB and use the app over WiFi (same LAN as Gridlock Standalone)." -ForegroundColor White
} else {
    Write-Host " Completed with errors. Failed: $($failed -join ', ')" -ForegroundColor Red
    exit 1
}
