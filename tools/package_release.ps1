param([string]$Version = '0.1.0')
# Builds the player download: the exe and runtime files only. Never the ROM,
# the config folder (it holds the stored ROM copy), logs, or debug files.
$ErrorActionPreference = 'Stop'
$root = 'C:\SpaceRanger\build-native'
$src = "$root\Release"
$name = "SpaceRanger-v$Version-windows-x64"
$stage = "$root\package\$name"
$out = "$stage\SpaceRanger"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $out | Out-Null
Copy-Item "$src\SpaceRanger.exe" "$out\SpaceRanger.exe"
foreach ($f in 'SDL2.dll', 'dxcompiler.dll', 'dxil.dll', 'freetype.dll') {
    Copy-Item "$src\$f" "$out\$f"
}
# Only approved assets ship (allowlist), never whatever else is in the folder.
$assetFiles = @('InterVariable.ttf', 'NotoEmoji-Regular.ttf', 'recomp.rcss', 'promptfont\promptfont.ttf', 'TS2Menu-Fredoka.ttf', 'launcher-background.svg', 'launcher-logo.svg', 'launcher-moon.svg', 'launcher-rocket.svg')
foreach ($a in $assetFiles) { $d = Join-Path "$out\assets" $a; New-Item -ItemType Directory -Path (Split-Path $d) -Force | Out-Null; Copy-Item (Join-Path "$src\assets" $a) $d }

# Windowed (GUI) subsystem so no console window opens next to the game.
$exe = "$out\SpaceRanger.exe"
$bytes = [IO.File]::ReadAllBytes($exe)
$pe = [BitConverter]::ToInt32($bytes, 0x3C)
if ([BitConverter]::ToUInt32($bytes, $pe) -ne 0x4550) { throw 'not a PE file' }
$bytes[$pe + 0x5C] = 2; $bytes[$pe + 0x5D] = 0
[IO.File]::WriteAllBytes($exe, $bytes)

Copy-Item 'C:\SpaceRanger\tools\release\README.txt' "$out\README.txt"
Copy-Item 'C:\SpaceRanger\tools\release\THIRD-PARTY-NOTICES.txt' "$out\THIRD-PARTY-NOTICES.txt"
Copy-Item 'C:\SpaceRanger\LICENSE' "$out\LICENSE.txt"
Copy-Item 'C:\SpaceRanger\tools\release\art\Fredoka-OFL.txt' "$out\assets\OFL-fonts.txt"

# Safety net: nothing ROM-like may ship.
$bad = Get-ChildItem $out -Recurse -File | Where-Object { $_.Extension -in '.z64', '.n64', '.v64', '.bin', '.log', '.pdb' -or $_.Length -eq 12582912 }
if ($bad) { throw "refusing to package: $($bad.FullName -join ', ')" }

$zip = "$root\package\$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
# Windows tar (bsdtar) writes standard "/" separators; PowerShell 5's
# Compress-Archive and .NET ZipFile write "\", which breaks some extractors.
& "$env:SystemRoot\System32\tar.exe" -a -c -f $zip -C $stage SpaceRanger
if ($LASTEXITCODE -ne 0) { throw "tar failed: $LASTEXITCODE" }
Get-ChildItem $out -Recurse -File | Select-Object @{n='File';e={$_.FullName.Substring($out.Length + 1)}}, Length | Format-Table -AutoSize | Out-String
"ZIP=$zip  " + (Get-Item $zip).Length + ' bytes'
"SHA256=" + (Get-FileHash $zip).Hash
