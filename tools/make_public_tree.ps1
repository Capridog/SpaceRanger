# Builds the public GitHub tree (curated: build inputs only, no ROM-derived
# code, no decomp/disassembly notes) in build-native\public-repo.
$ErrorActionPreference = 'Stop'
$Root = 'C:\SpaceRanger'
$Pub = "$Root\build-native\public-repo"
if (-not (Test-Path "$Pub\.git")) {
    git clone https://github.com/Capridog/ToyStory2Recompiled.git $Pub
}
Get-ChildItem $Pub -Force | Where-Object { $_.Name -ne '.git' } | Remove-Item -Recurse -Force

$files = @(
    '.gitignore', 'CMakeLists.txt', 'rom\README.md', 'rsp\ts2_audio.toml',
    'config\toystory2.us.rev0.recomp.toml', 'config\toystory2.us.rev0.syms.toml',
    'config\toystory2.us.rev0.splat.auto.yaml', 'config\toystory2.us.rev0.splat.working.yaml',
    'tools\run_overlays_pass10.ps1', 'tools\audit_native_boundaries.py', 'tools\audit_text_as_functions.py',
    'tools\audit_unhandled_cache.py', 'tools\audit_lowmem_overlay_table.py', 'tools\map_lowmem_overlays.py',
    'tools\audit_func_80098B60.py', 'tools\audit_recomp_refs.py',
    'tools\test_audio_rsp.cpp', 'tools\test_audio_pacing.cpp', 'tools\test_audio_replay.cpp',
    'tools\audio_rsp_scalar.cpp', 'tools\test_frame_matching.cpp', 'tools\test_ts2_projection.cpp',
    'tools\replay_test.ps1', 'tools\package_release.ps1', 'tools\export_dep_patches.ps1', 'tools\make_public_tree.ps1',
    'tools\release\README.txt', 'tools\release\THIRD-PARTY-NOTICES.txt'
)
$dirs = @('src', 'include', 'assets', 'patches')
foreach ($f in $files) {
    $dst = Join-Path $Pub $f
    New-Item -ItemType Directory -Path (Split-Path $dst) -Force | Out-Null
    Copy-Item (Join-Path $Root $f) $dst
}
foreach ($d in $dirs) { Copy-Item (Join-Path $Root $d) (Join-Path $Pub $d) -Recurse }
Get-ChildItem $Pub -Recurse -File -Include '*.bak*', '*_backup', '*.orig', '*.rej', '*.before-*' | Remove-Item -Force
Remove-Item "$Pub\patches\deps\ts2-*.patch" -Force  # superseded snapshots; deps_*.patch are current
Copy-Item "$Root\tools\release\public\README.md" "$Pub\README.md"
New-Item -ItemType Directory "$Pub\docs" -Force | Out-Null
Copy-Item "$Root\tools\release\public\docs\BUILDING.md" "$Pub\docs\BUILDING.md"

# Safety net: no ROM images, generated recomp output or large binaries.
$bad = Get-ChildItem $Pub -Recurse -File -Force | Where-Object {
    $_.FullName -notlike "$Pub\.git\*" -and (
        $_.Extension -in '.z64', '.n64', '.v64', '.bin', '.exe', '.dll' -or
        $_.Name -like 'funcs_*.c' -or $_.Length -gt 2MB)
}
if ($bad) { throw "refusing: $($bad.FullName -join ', ')" }
git -C $Pub add -A
git -C $Pub status --short | Measure-Object | ForEach-Object { "files staged: $($_.Count)" }
