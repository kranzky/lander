# Build script for Lander release on Windows
# Creates a zip ready for itch.io distribution

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $ProjectDir "build-release"
$DistDir = Join-Path $ProjectDir "dist"

# Configuration - adjust these paths for your system
$SDL2Dir = "C:/Users/kranzky/dev/SDL2/i686-w64-mingw32"
$MingwMake = "C:/Users/kranzky/dev/mingw32/bin/mingw32-make.exe"
$Butler = "C:/Users/kranzky/dev/butler/butler.exe"

Write-Host "=== Lander Windows Release Build ===" -ForegroundColor Cyan
Write-Host "Project: $ProjectDir"
Write-Host ""

# Step 1: Create directories
Write-Host "Step 1: Creating directories..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

# Step 2: Configure CMake
Write-Host "Step 2: Configuring CMake..." -ForegroundColor Yellow
cmake -G "MinGW Makefiles" `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH="$SDL2Dir" `
    -S "$ProjectDir" `
    -B "$BuildDir"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: CMake configuration failed" -ForegroundColor Red
    exit 1
}

# Step 3: Build release binary
Write-Host "Step 3: Building release binary..." -ForegroundColor Yellow
& $MingwMake -j4 -C "$BuildDir" lander

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Build failed" -ForegroundColor Red
    exit 1
}

# Step 4: Copy files to dist
Write-Host "Step 4: Copying files to dist..." -ForegroundColor Yellow
Copy-Item -Force (Join-Path $BuildDir "lander.exe") $DistDir
Copy-Item -Force (Join-Path $SDL2Dir "bin/SDL2.dll") $DistDir

# Step 5: Create zip
Write-Host "Step 5: Creating distribution archive..." -ForegroundColor Yellow
$ZipPath = Join-Path $DistDir "Lander-Windows.zip"
Remove-Item -Force $ZipPath -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $DistDir "lander.exe"), (Join-Path $DistDir "SDL2.dll") `
    -DestinationPath $ZipPath -Force

Write-Host ""
Write-Host "=== Build Complete ===" -ForegroundColor Green
Write-Host "Distribution zip: $ZipPath"
Write-Host ""
Write-Host "To distribute, run:" -ForegroundColor Cyan
Write-Host "  $Butler push `"$ZipPath`" kranzky/lander:windows --userversion VERSION"
