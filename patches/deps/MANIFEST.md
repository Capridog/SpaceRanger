# Dependency snapshot manifest

Each dependency is the upstream repository at the base commit plus the
matching patch in this folder (`git apply` inside that directory).

| directory | upstream | base commit | SpaceRanger commit |
| --- | --- | --- | --- |
| deps/rt64 | https://github.com/rt64/rt64 | 5473732a822a4423b5696e7cb18fecc425a59875 | 92a5425ca44c8f343f8b2fbb7e3c6c907efd2657 |
| deps/rt64/src/contrib/plume | https://github.com/renderbag/plume | d890ac899e505fb30040e037a4037cdeca68f033 | 30734dfb75db591b3b24f537a61ba7f579127198 |
| deps/N64ModernRuntime | https://github.com/N64Recomp/N64ModernRuntime | 589bbf018a3e6d3646ddf7de1e7919f1b7e99bb1 | 3c828694762879a7800e5545d1a59f234faad030 |
| deps/N64ModernRuntime/N64Recomp | https://github.com/N64Recomp/N64Recomp | 81213c1831fab2521a6a5459c67b63437d67e253 | 635e358e52322cf3c80df6afdc821fcaad79e197 |
| deps/RecompFrontend | https://github.com/N64Recomp/RecompFrontend | b1a1477c6556aeb7ed45defbfb5924f721efebc1 | 29a5a1501f50d5c8f61ffcdcfd50046e6dfc4a48 |
| tools/N64Recomp | https://github.com/N64Recomp/N64Recomp | ffb39cdad1da5de07eaaa48bd1db4a89a7986771 | 8cec62aac1ffb1b1ac1b61c3e06eb9d4145415ad |
