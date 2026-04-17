#Requires -Version 5.1
<#
.SYNOPSIS
    One-click build and run for AI-enhanced Notepad++ (claude-develop branch).
    Run this on Windows 10/11. No manual steps needed.

.DESCRIPTION
    1. Self-elevates to admin
    2. Installs Git + MSYS2 via winget (silently)
    3. Installs MinGW-w64 toolchain via pacman
    4. Clones the claude-develop branch
    5. Builds notepad++.exe
    6. Launches it

    AI endpoint: point to http://<your-ollama-host>:11434
    Set in: Settings -> Preferences -> AI Assistant
#>

# ─── Self-elevate ────────────────────────────────────────────────────────────
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "Restarting as Administrator..." -ForegroundColor Yellow
    Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    exit
}

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ─── Config ──────────────────────────────────────────────────────────────────
$REPO_URL    = "https://github.com/charlesaurav13/notepad-plus-plus.git"
$BRANCH      = "claude-develop"
$INSTALL_DIR = "C:\NotepadAI"
$MSYS2_DIR   = "C:\msys64"
$BASH        = "$MSYS2_DIR\usr\bin\bash.exe"

# ─── Helpers ─────────────────────────────────────────────────────────────────
function Step  { param($m) Write-Host "`n>>> $m" -ForegroundColor Cyan }
function OK    { param($m) Write-Host "    [OK] $m" -ForegroundColor Green }
function Warn  { param($m) Write-Host "    [!!] $m" -ForegroundColor Yellow }
function Die   { param($m) Write-Host "`n[FAILED] $m" -ForegroundColor Red; Read-Host "`nPress Enter to exit"; exit 1 }

function Refresh-Path {
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" +
                [System.Environment]::GetEnvironmentVariable("Path","User")
}

function Winget-Install {
    param($id, $name)
    Step "Checking $name..."
    $already = winget list --id $id 2>$null | Select-String $id
    if ($already) { OK "$name already installed"; return }
    Write-Host "    Installing $name (this may take a few minutes)..."
    winget install --id $id -e --silent --accept-source-agreements --accept-package-agreements
    if ($LASTEXITCODE -ne 0) { Die "Failed to install $name via winget." }
    Refresh-Path
    OK "$name installed"
}

# ─── 1. winget check ─────────────────────────────────────────────────────────
Step "Checking winget..."
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Die "winget not found.`nOn Windows 11 it comes pre-installed.`nOn Windows 10: install 'App Installer' from the Microsoft Store."
}
OK "winget available"

# ─── 2. Git ──────────────────────────────────────────────────────────────────
Winget-Install "Git.Git" "Git"

# Ensure git.exe is on PATH
$gitExe = (Get-Command git -ErrorAction SilentlyContinue)?.Source
if (-not $gitExe) {
    $gitExe = "$env:ProgramFiles\Git\cmd\git.exe"
    if (-not (Test-Path $gitExe)) { Die "git.exe not found after install. Reboot and re-run." }
    $env:Path = "$env:ProgramFiles\Git\cmd;" + $env:Path
}
OK "git at: $gitExe"

# ─── 3. MSYS2 ────────────────────────────────────────────────────────────────
Winget-Install "MSYS2.MSYS2" "MSYS2"

if (-not (Test-Path $BASH)) {
    # winget may have installed to a different location — try to find it
    $found = Get-ChildItem "C:\msys*" -Filter "usr\bin\bash.exe" -ErrorAction SilentlyContinue |
             Select-Object -First 1
    if ($found) { $BASH = $found.FullName; $MSYS2_DIR = Split-Path (Split-Path (Split-Path $found.FullName)) }
    else { Die "MSYS2 bash.exe not found at $BASH. Try re-running or install MSYS2 manually from https://www.msys2.org" }
}
OK "MSYS2 at: $MSYS2_DIR"

# ─── 4. MinGW-w64 toolchain via pacman ───────────────────────────────────────
Step "Installing MinGW-w64 toolchain (gcc, make, windres)..."
$pkgs = "mingw-w64-x86_64-gcc mingw-w64-x86_64-binutils mingw-w64-x86_64-make"

# First update pacman itself (silent)
& $BASH -lc "pacman -Sy --noconfirm" 2>&1 | Out-Null

# Install packages
& $BASH -lc "pacman -S --noconfirm --needed $pkgs"
if ($LASTEXITCODE -ne 0) { Die "pacman install failed." }
OK "MinGW-w64 toolchain ready"

# ─── 5. Clone / update repo ──────────────────────────────────────────────────
Step "Getting Notepad++ source ($BRANCH)..."
if (Test-Path "$INSTALL_DIR\.git") {
    Warn "Repo exists — pulling latest..."
    & $gitExe -C $INSTALL_DIR fetch --quiet origin
    & $gitExe -C $INSTALL_DIR checkout $BRANCH
    & $gitExe -C $INSTALL_DIR pull --quiet origin $BRANCH
    OK "Repo updated"
} else {
    New-Item -ItemType Directory -Force -Path $INSTALL_DIR | Out-Null
    Write-Host "    Cloning (this takes a minute)..."
    & $gitExe clone --depth 1 -b $BRANCH $REPO_URL $INSTALL_DIR
    if ($LASTEXITCODE -ne 0) { Die "git clone failed." }
    OK "Repo cloned to $INSTALL_DIR"
}

# ─── 6. Build ────────────────────────────────────────────────────────────────
Step "Building Notepad++ (5-15 min first time)..."

# Convert Windows path to MSYS2 path  (C:\NotepadAI -> /c/NotepadAI)
$msysInstallDir = "/" + ($INSTALL_DIR -replace "\\", "/" -replace "^([A-Za-z]):/", '$1/').ToLower().TrimStart("/")
# Simpler: C:\NotepadAI -> /c/NotepadAI
$msysInstallDir = "/" + $INSTALL_DIR[0].ToString().ToLower() + ($INSTALL_DIR.Substring(2) -replace "\\", "/")

$buildScript = @"
set -e
export PATH="/mingw64/bin:`$PATH"
cd "$msysInstallDir/PowerEditor/gcc"
echo "=== Toolchain: `$(gcc --version | head -1) ==="
mingw32-make -j4
"@

& $BASH -lc $buildScript
if ($LASTEXITCODE -ne 0) { Die "Build failed. Scroll up to see the error." }
OK "Build complete"

# ─── 7. Find binary ──────────────────────────────────────────────────────────
Step "Locating notepad++.exe..."
$exe = Get-ChildItem -Path "$INSTALL_DIR\PowerEditor\gcc" -Recurse -Filter "notepad++.exe" -ErrorAction SilentlyContinue |
       Where-Object { $_.FullName -notmatch "\\obj\\" } |
       Select-Object -First 1

if (-not $exe) { Die "notepad++.exe not found. Check build output above." }
OK "Binary: $($exe.FullName)"

# ─── 8. Launch ───────────────────────────────────────────────────────────────
Step "Launching Notepad++..."
Start-Process $exe.FullName

Write-Host @"

╔══════════════════════════════════════════════════════════╗
║        Notepad++ AI Edition is running!                  ║
║                                                          ║
║  To activate AI features:                               ║
║  Settings → Preferences → AI Assistant                  ║
║                                                          ║
║  Endpoint : http://192.168.1.25:11434                   ║
║  Model    : deepseek-r1:7b                              ║
║                                                          ║
║  Features:                                               ║
║  • Right-click → AI Assistant (explain/fix/refactor)    ║
║  • View → AI Assistant Panel (chat sidebar)             ║
║  • Ctrl+Space → inline AI completion                    ║
╚══════════════════════════════════════════════════════════╝

"@ -ForegroundColor Green
