param([string]$Version = '0.1.0')
# Builds the player download: the exe and runtime files only. Never the ROM,
# the config folder (it holds the stored ROM copy), logs, or debug files.
$ErrorActionPreference = 'Stop'
$root = 'C:\SpaceRanger\build-native'
$src = "$root\Release"
$name = "ToyStory2Recompiled-v$Version-windows-x64"
$out = "$root\package\$name"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Path $out | Out-Null
foreach ($f in 'SpaceRanger.exe', 'SDL2.dll', 'dxcompiler.dll', 'dxil.dll', 'freetype.dll') {
    Copy-Item "$src\$f" "$out\$f"
}
Copy-Item "$src\assets" "$out\assets" -Recurse

# Windowed (GUI) subsystem so no console window opens next to the game.
$exe = "$out\SpaceRanger.exe"
$bytes = [IO.File]::ReadAllBytes($exe)
$pe = [BitConverter]::ToInt32($bytes, 0x3C)
if ([BitConverter]::ToUInt32($bytes, $pe) -ne 0x4550) { throw 'not a PE file' }
$bytes[$pe + 0x5C] = 2; $bytes[$pe + 0x5D] = 0
[IO.File]::WriteAllBytes($exe, $bytes)

Copy-Item 'C:\SpaceRanger\tools\release\README.txt' "$out\README.txt"
Copy-Item 'C:\SpaceRanger\tools\release\THIRD-PARTY-NOTICES.txt' "$out\THIRD-PARTY-NOTICES.txt"

# Safety net: nothing ROM-like may ship.
$bad = Get-ChildItem $out -Recurse -File | Where-Object { $_.Extension -in '.z64', '.n64', '.v64', '.bin', '.log', '.pdb' -or $_.Length -eq 12582912 }
if ($bad) { throw "refusing to package: $($bad.FullName -join ', ')" }

$zip = "$root\package\$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path "$out\*" -DestinationPath $zip
Get-ChildItem $out -Recurse -File | Select-Object @{n='File';e={$_.FullName.Substring($out.Length + 1)}}, Length | Format-Table -AutoSize | Out-String
"ZIP=$zip  " + (Get-Item $zip).Length + ' bytes'
"SHA256=" + (Get-FileHash $zip).Hash
