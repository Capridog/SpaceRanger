# Exports each dependency's SpaceRanger changes (upstream base -> current
# "spaceranger" HEAD) to patches/deps, and rewrites MANIFEST.md.
$ErrorActionPreference = 'Stop'
$Root = 'C:\SpaceRanger'
$deps = @(
    @{ Dir = 'deps\rt64'; Base = '5473732a822a4423b5696e7cb18fecc425a59875'; Patch = 'deps_rt64.patch'; Url = 'https://github.com/rt64/rt64' },
    @{ Dir = 'deps\rt64\src\contrib\plume'; Base = 'd890ac899e505fb30040e037a4037cdeca68f033'; Patch = 'deps_rt64_src_contrib_plume.patch'; Url = 'https://github.com/renderbag/plume' },
    @{ Dir = 'deps\N64ModernRuntime'; Base = '589bbf018a3e6d3646ddf7de1e7919f1b7e99bb1'; Patch = 'deps_N64ModernRuntime.patch'; Url = 'https://github.com/N64Recomp/N64ModernRuntime' },
    @{ Dir = 'deps\N64ModernRuntime\N64Recomp'; Base = '81213c1831fab2521a6a5459c67b63437d67e253'; Patch = 'deps_N64ModernRuntime_N64Recomp.patch'; Url = 'https://github.com/N64Recomp/N64Recomp' },
    @{ Dir = 'deps\RecompFrontend'; Base = 'b1a1477c6556aeb7ed45defbfb5924f721efebc1'; Patch = 'deps_RecompFrontend.patch'; Url = 'https://github.com/N64Recomp/RecompFrontend' },
    @{ Dir = 'tools\N64Recomp'; Base = 'ffb39cdad1da5de07eaaa48bd1db4a89a7986771'; Patch = 'tools_N64Recomp.patch'; Url = 'https://github.com/N64Recomp/N64Recomp' }
)
$rows = @()
foreach ($d in $deps) {
    $dir = Join-Path $Root $d.Dir
    $dirty = git -C $dir status --porcelain --ignore-submodules=all --untracked-files=no
    if ($dirty) { throw "$($d.Dir) has uncommitted changes; commit them first." }
    $head = (git -C $dir rev-parse HEAD).Trim()
    $url = $d.Url
    $out = Join-Path $Root "patches\deps\$($d.Patch)"
    $text = (git -C $dir -c core.autocrlf=false diff --binary --ignore-submodules=all $d.Base $head) -join "`n"
    [IO.File]::WriteAllText($out, $text + "`n", (New-Object Text.UTF8Encoding($false)))
    $rows += "| $($d.Dir.Replace('\','/')) | $url | $($d.Base) | $head |"
    "{0,-34} {1}  {2} KB" -f $d.Dir, $head.Substring(0, 8), [int]((Get-Item $out).Length / 1KB)
}
$manifest = @(
    '# Dependency snapshot manifest', '',
    'Each dependency is the upstream repository at the base commit plus the',
    'matching patch in this folder (`git apply` inside that directory).', '',
    '| directory | upstream | base commit | SpaceRanger commit |', '| --- | --- | --- | --- |'
) + $rows
[IO.File]::WriteAllText((Join-Path $Root 'patches\deps\MANIFEST.md'), ($manifest -join "`n") + "`n", (New-Object Text.UTF8Encoding($false)))
