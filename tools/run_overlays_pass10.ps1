$ErrorActionPreference = 'Stop'
$Root = 'C:\SpaceRanger'
$Python = Join-Path $Root '.venv\Scripts\python.exe'
$Exe = Join-Path $Root 'build\N64Recomp\Release\N64Recomp.exe'
$OriginalConfig = Join-Path $Root 'config\toystory2.us.rev0.recomp.toml'
$RunId = 'pass10-overlays-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
$Config = Join-Path $Root "config\$RunId.recomp.toml"
$Output = Join-Path $Root "generated\$RunId"
$Stdout = Join-Path $Root "docs\$RunId.stdout.log"
$Stderr = Join-Path $Root "docs\$RunId.stderr.log"
$Log = Join-Path $Root "docs\$RunId.log"
$Utf8 = New-Object System.Text.UTF8Encoding($false)

foreach ($Required in @($Python, $Exe, $OriginalConfig)) {
    if (-not (Test-Path -LiteralPath $Required -PathType Leaf)) {
        throw "Missing required file: $Required"
    }
}

# Static preflight. Each tool is self-contained (ROM + config only) and
# returns non-zero on any hard failure, so a bad map never reaches N64Recomp.
& $Python "$Root\tools\audit_native_boundaries.py"
if ($LASTEXITCODE -ne 0) { throw 'Native boundary audit failed; recompilation was not started.' }

& $Python "$Root\tools\audit_text_as_functions.py"
if ($LASTEXITCODE -ne 0) { throw 'Text-as-function audit failed; recompilation was not started.' }

& $Python "$Root\tools\audit_unhandled_cache.py" | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Unsupported cache instruction audit failed; recompilation was not started.' }

& $Python "$Root\tools\audit_lowmem_overlay_table.py" | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Overlay table audit failed; recompilation was not started.' }

& $Python "$Root\tools\map_lowmem_overlays.py" | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Overlay function map failed closure audit; recompilation was not started.' }

& $Python "$Root\tools\audit_func_80098B60.py" | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'func_80098B60 boundary audit failed; recompilation was not started.' }

$Text = [System.IO.File]::ReadAllText($OriginalConfig)
$Pattern = '(?m)^output_func_path\s*=\s*"[^"\r\n]*"\s*$'
if ([regex]::Matches($Text, $Pattern).Count -ne 1) {
    throw 'Expected exactly one output_func_path setting.'
}
$Text = [regex]::Replace($Text, $Pattern, "output_func_path = `"../generated/$RunId`"")
if ((Test-Path -LiteralPath $Config) -or (Test-Path -LiteralPath $Output)) {
    throw "Diagnostic run already exists: $RunId"
}
[System.IO.File]::WriteAllText($Config, $Text, $Utf8)
New-Item -ItemType Directory -Path $Output | Out-Null

$Proc = Start-Process -FilePath $Exe -ArgumentList @($Config) `
    -WorkingDirectory "$Root\config" -WindowStyle Hidden `
    -RedirectStandardOutput $Stdout -RedirectStandardError $Stderr `
    -Wait -PassThru
$FullText = "N64Recomp exit code: $($Proc.ExitCode)`r`n" +
    "Config: $Config`r`nOutput: $Output`r`n`r`nSTDOUT`r`n" +
    [System.IO.File]::ReadAllText($Stdout) + "`r`nSTDERR`r`n" +
    [System.IO.File]::ReadAllText($Stderr)
[System.IO.File]::WriteAllText($Log, $FullText, $Utf8)
Write-Output $FullText
Write-Output "Full log: $Log"
if ($Proc.ExitCode -ne 0) {
    throw "N64Recomp reached a blocker (exit $($Proc.ExitCode)). See $Log"
}
& $Python "$Root\tools\audit_recomp_refs.py" $Output
if ($LASTEXITCODE -ne 0) { throw "Generated code still calls unsupported scheduler internals. See $Output" }
