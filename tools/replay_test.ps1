# Automated replay test: runs the game from boot with recorded inputs and
# writes per-displayed-frame metrics (TS2_DISPLAY_METRICS) for analysis.
#   powershell -File tools\replay_test.ps1 -Name fp-arm [-Env @{TS2_X='1'}] [-Video] [-Fullscreen]
param(
    [string]$Name = 'replay',
    [string]$Exe = 'SpaceRanger.smooth-20260923.exe',
    [string]$Inputs = 'fp-test.inputs',
    [hashtable]$Env = @{},
    [int]$MaxSeconds = 300,
    [switch]$Video,
    [switch]$Fullscreen
)
$ErrorActionPreference = 'Stop'
$Dir = 'C:\SpaceRanger\build-native\RenderTest'
$OutDir = 'C:\SpaceRanger\build-native\replay-runs'
New-Item -ItemType Directory -Force $OutDir | Out-Null
$Stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$Base = Join-Path $OutDir "$Name-$Stamp"
$Graphics = Join-Path $Dir 'spaceranger-config\graphics.json'
$GraphicsBackup = "$Graphics.replay-backup"

$env:TS2_INPUT_RECORD = ''
$env:TS2_INPUT_REPLAY = Join-Path $Dir $Inputs
$env:TS2_REPLAY_EXIT = '1'
$env:TS2_TRANSITION_HARD_CUT = '1'
$env:TS2_VISIBILITY_DIAGNOSTICS = ''
$env:TS2_DISPLAY_METRICS = "$Base.metrics.csv"
foreach ($k in $Env.Keys) { Set-Item "env:$k" $Env[$k] }

if ($Fullscreen) {
    Copy-Item $Graphics $GraphicsBackup -Force
    (Get-Content $Graphics) -replace '"wm_option": "Windowed"', '"wm_option": "Fullscreen"' | Set-Content $Graphics
}
# Keep other windows (e.g. the Claude app) from covering the game capture.
(New-Object -ComObject Shell.Application).MinimizeAll()
Start-Sleep -Milliseconds 800
try {
    $game = Start-Process -FilePath (Join-Path $Dir $Exe) -WorkingDirectory $Dir -PassThru `
        -RedirectStandardError "$Base.log" -RedirectStandardOutput "$Base.out.log"
    $rec = $null
    if ($Video) {
        $ffmpeg = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter ffmpeg.exe -ErrorAction SilentlyContinue | Select -First 1 -ExpandProperty FullName
        Start-Sleep -Seconds 3
        $rec = Start-Process -FilePath $ffmpeg -PassThru -WindowStyle Hidden -RedirectStandardError "$Base.ffmpeg.log" -ArgumentList @(
            '-y', '-loglevel', 'error', '-f', 'lavfi', '-i', 'ddagrab=framerate=60,hwdownload,format=bgra',
            '-t', "$($MaxSeconds + 5)", '-vf', 'scale=960:-2', '-c:v', 'libx264', '-preset', 'ultrafast', '-crf', '20',
            '-pix_fmt', 'yuv420p', "$Base.mkv")
    }
    $deadline = (Get-Date).AddSeconds($MaxSeconds)
    while (-not $game.HasExited -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 500 }
    if (-not $game.HasExited) { Stop-Process $game -Force; "TIMEOUT" }
    if ($rec) {
        # Let ffmpeg finish on its own duration limit so the file is complete.
        $wait = (Get-Date).AddSeconds(15)
        while (-not $rec.HasExited -and (Get-Date) -lt $wait) { Start-Sleep -Milliseconds 500 }
        if (-not $rec.HasExited) { Stop-Process $rec -Force }
    }
}
finally {
    if ($Fullscreen -and (Test-Path $GraphicsBackup)) { Copy-Item $GraphicsBackup $Graphics -Force }
}
"METRICS=$Base.metrics.csv"
"LOG=$Base.log"
if ($Video) { "VIDEO=$Base.mkv" }
