# Build, code sign and zip Lander for Windows, ready for itch.io.
#
#   .\scripts\build_windows.ps1
#
# Signing uses SSL.com eSigner cloud signing via CodeSignTool, as in
# MultiMouse/Lineage. It runs when these environment variables are set
# (values from the password manager / SSL.com dashboard):
#   ES_USERNAME       SSL.com account email
#   ES_PASSWORD       SSL.com account password
#   ES_CREDENTIAL_ID  eSigner credential ID
#   ES_TOTP_SECRET    eSigner TOTP automation secret
# Each signing uses eSigner quota (shared with the other projects).
#
# CodeSignTool is downloaded on first use. It needs Java 17 or newer (java on
# PATH, or JAVA_HOME): the Java 11.0.2 bundled with it no longer trusts
# SSL.com's servers ("PKIX path building failed").
#
# Without ES_USERNAME the build is left unsigned and zipped as
# Lander-Windows-unsigned.zip, so it can't be mistaken for a release.

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $ProjectDir "build-release"
$DistDir = Join-Path $ProjectDir "dist"
$SignedDir = Join-Path $BuildDir "signed"

# Configuration - adjust these paths for your system
$SDL2Dir = "C:/Users/kranzky/dev/SDL2/i686-w64-mingw32"
$MingwMake = "C:/Users/kranzky/dev/mingw32/bin/mingw32-make.exe"
$Butler = "C:/Users/kranzky/dev/butler/butler.exe"
$CodeSignToolVersion = "v1.3.2"

$Version = (Select-String -Path (Join-Path $ProjectDir "CMakeLists.txt") `
    -Pattern 'project\(lander VERSION ([0-9.]+)').Matches[0].Groups[1].Value
$Sign = [bool]$env:ES_USERNAME
$Suffix = if ($Sign) { "" } else { "-unsigned" }

function Invoke-Checked([string]$What, [scriptblock]$Command) {
    & $Command
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: $What failed" -ForegroundColor Red
        exit 1
    }
}

# Download CodeSignTool (once) and return its directory
function Get-CodeSignTool {
    $tool = Join-Path $BuildDir "CodeSignTool-$CodeSignToolVersion"
    if (!(Test-Path $tool)) {
        $zip = "$tool.zip"
        $url = "https://github.com/SSLcom/CodeSignTool/releases/download/$CodeSignToolVersion/CodeSignTool-$CodeSignToolVersion-windows.zip"
        Invoke-WebRequest $url -OutFile $zip
        Expand-Archive $zip -DestinationPath $tool -Force
        Remove-Item $zip
    }
    return $tool
}

# Sign a file in place with eSigner and verify the signature
function Invoke-CodeSign([string]$File) {
    $tool = Get-CodeSignTool
    $jar = Get-ChildItem (Join-Path $tool "jar/code_sign_tool-*.jar") | Select-Object -First 1
    if (!$jar) { throw "CodeSignTool jar not found in $tool" }
    $java = if ($env:JAVA_HOME) { Join-Path $env:JAVA_HOME "bin/java.exe" } else { "java" }

    # CodeSignTool refuses to write into the input's own directory
    New-Item -ItemType Directory -Force -Path $SignedDir | Out-Null

    # CodeSignTool reads its config relative to the working directory
    $env:CODE_SIGN_TOOL_PATH = $tool
    Push-Location $tool
    try {
        & $java -jar $jar.FullName sign `
            "-username=$env:ES_USERNAME" "-password=$env:ES_PASSWORD" `
            "-credential_id=$env:ES_CREDENTIAL_ID" "-totp_secret=$env:ES_TOTP_SECRET" `
            "-input_file_path=$File" "-output_dir_path=$SignedDir" `
            "-override=true" "-malware_block=false"
        if ($LASTEXITCODE -ne 0) { throw "CodeSignTool exited with $LASTEXITCODE" }
    } finally {
        Pop-Location
    }

    Copy-Item -Force (Join-Path $SignedDir (Split-Path -Leaf $File)) $File
    $sig = Get-AuthenticodeSignature $File
    Write-Host "Signature: $($sig.Status), signer: $($sig.SignerCertificate.Subject)"
    if ($sig.Status -ne 'Valid') { throw "$File signature is not valid: $($sig.Status)" }
}

Write-Host "=== Lander $Version Windows Release Build ===" -ForegroundColor Cyan
if (!$Sign) {
    Write-Host "WARNING: ES_USERNAME is not set; building UNSIGNED" -ForegroundColor Yellow
}

Write-Host "Step 1: Configuring CMake..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path $BuildDir, $DistDir | Out-Null
Invoke-Checked "CMake configuration" {
    cmake -G "MinGW Makefiles" `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_PREFIX_PATH="$SDL2Dir" `
        -S "$ProjectDir" `
        -B "$BuildDir"
}

Write-Host "Step 2: Building release binary..." -ForegroundColor Yellow
Invoke-Checked "Build" { & $MingwMake -j4 -C "$BuildDir" lander }

Write-Host "Step 3: Copying files to dist..." -ForegroundColor Yellow
$Exe = Join-Path $DistDir "lander.exe"
Copy-Item -Force (Join-Path $BuildDir "lander.exe") $Exe
Copy-Item -Force (Join-Path $SDL2Dir "bin/SDL2.dll") $DistDir
Copy-Item -Force -Recurse (Join-Path $ProjectDir "sounds") (Join-Path $DistDir "sounds")

if ($Sign) {
    Write-Host "Step 4: Signing lander.exe (eSigner)..." -ForegroundColor Yellow
    Invoke-CodeSign $Exe
}

Write-Host "Step 5: Creating distribution archive..." -ForegroundColor Yellow
Remove-Item -Force (Join-Path $DistDir "Lander-Windows*.zip") -ErrorAction SilentlyContinue
$ZipPath = Join-Path $DistDir "Lander-Windows$Suffix.zip"
Compress-Archive -Path $Exe, (Join-Path $DistDir "SDL2.dll"), (Join-Path $DistDir "sounds") `
    -DestinationPath $ZipPath -Force

Write-Host ""
Write-Host "=== Build Complete ===" -ForegroundColor Green
Write-Host "Distribution zip: $ZipPath"
if ($Sign) {
    Write-Host ""
    Write-Host "To distribute, run:" -ForegroundColor Cyan
    Write-Host "  $Butler push `"$ZipPath`" kranzky/lander:windows --userversion $Version"
} else {
    Write-Host "UNSIGNED build (not for release)" -ForegroundColor Yellow
}
